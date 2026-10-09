#include "hc06.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>
#include "app_tasks.h"
#include "bsp_uart.h"
#include "bsp_flash.h"


volatile HC06_Time_t g_bt_time = {0};

volatile uint8_t g_bt_time_valid = 0;

volatile uint8_t g_bt_time_updated = 0;

volatile uint8_t g_bt_debug_buf[3] = {0};

volatile uint8_t g_bt_debug_ready = 0;

volatile uint8_t g_bt_rx_count = 0;

volatile HC06_Alarm_t g_bt_alarms[3] = {
    {ALARM_HOUR_1, ALARM_MINUTE_1, 1},
    {ALARM_HOUR_2, ALARM_MINUTE_2, 1},
    {ALARM_HOUR_3, ALARM_MINUTE_3, 1}
};

volatile uint8_t g_bt_alarm_updated = 0;

volatile uint8_t g_bt_alarm_index = 0;

/* 闹钟配置脏标志：指令解析处只置位，真正的 Flash 擦写统一由 HC06_Poll()
 * 在任务上下文执行。这样 USART1 中断里不再出现 FLASH_ErasePage 与
 * taskENTER_CRITICAL()（后者在 ISR 中本来就是非法用法）。 */
static volatile uint8_t g_bt_alarm_dirty = 0;

static uint16_t Alarm_Pack(const volatile HC06_Alarm_t *alarm)
{
    return (uint16_t)alarm->hour |
           ((uint16_t)alarm->minute << 8) |
           ((uint16_t)(alarm->enabled ? 1U : 0U) << 15);
}

static uint8_t Alarm_DataValid(uint16_t word)
{
    uint8_t hour = (uint8_t)(word & 0xFFU);
    uint8_t minute = (uint8_t)((word >> 8) & 0x7FU);
    return (hour <= 23U && minute <= 59U);
}

/* 从 Flash 读取并校验已保存的三组闹钟。 */
static void HC06_LoadAlarms(void)
{
    uint16_t words[BSP_FLASH_ALARM_WORD_COUNT];
    uint8_t i;

    /* BSP_Flash_ReadAlarms 已经做过 magic 与校验和的检查 */
    if(!BSP_Flash_ReadAlarms(words))
    {
        return;
    }

    for(i = 0; i < 3U; i++)
    {
        if(!Alarm_DataValid(words[i + 1U]))
        {
            return;
        }
    }

    for(i = 0; i < 3U; i++)
    {
        uint16_t word = words[i + 1U];
        g_bt_alarms[i].hour = (uint8_t)(word & 0xFFU);
        g_bt_alarms[i].minute = (uint8_t)((word >> 8) & 0x7FU);
        g_bt_alarms[i].enabled = (uint8_t)((word >> 15) & 0x01U);
    }
}

/* 将当前三组闹钟写入 Flash，供下次上电恢复。
 * 打包仍在临界区内完成（保证三个闹钟字段一致），擦写动作交给 BSP Flash 层。 */
void HC06_SaveAlarms(void)
{
    uint16_t words[BSP_FLASH_ALARM_WORD_COUNT];

    taskENTER_CRITICAL();
    words[0] = BSP_FLASH_ALARM_MAGIC;
    words[1] = Alarm_Pack(&g_bt_alarms[0]);
    words[2] = Alarm_Pack(&g_bt_alarms[1]);
    words[3] = Alarm_Pack(&g_bt_alarms[2]);
    words[4] = BSP_Flash_AlarmChecksum(words);
    taskEXIT_CRITICAL();

    (void)BSP_Flash_WriteAlarms(words);
}



/* 解析蓝牙三字节协议：时间同步或闹钟设置。 */
void HC06_ProcessByte(uint8_t byte)
{
    static uint8_t stage = 0;
    static uint8_t temp_buf[3];
    static uint32_t last_byte_tick = 0;
    uint32_t now = xTaskGetTickCount();

    uint8_t first, second, third;
    uint8_t alarm_index;


    if((now - last_byte_tick) > pdMS_TO_TICKS(500) && stage != 0) {
        stage = 0;
    }
    last_byte_tick = now;


    g_bt_debug_buf[g_bt_rx_count % 3] = byte;
    g_bt_rx_count++;


    switch(stage) {
        case 0:
            temp_buf[0] = byte;
            stage = 1;
            break;

        case 1:
            temp_buf[1] = byte;
            stage = 2;
            break;

        case 2:
            temp_buf[2] = byte;
            stage = 0;

            first = temp_buf[0];
            second = temp_buf[1];
            third = temp_buf[2];


            g_bt_debug_buf[0] = first;
            g_bt_debug_buf[1] = second;
            g_bt_debug_buf[2] = third;
            g_bt_debug_ready = 1;


            if(first >= 24) {

                alarm_index = first - 24;


                if(alarm_index < 3 && second <= 23 && third <= 59) {
                    taskENTER_CRITICAL();
                    g_bt_alarms[alarm_index].hour = second;
                    g_bt_alarms[alarm_index].minute = third;
                    g_bt_alarms[alarm_index].enabled = 1;
                    g_bt_alarm_updated = 1;
                    g_bt_alarm_index = alarm_index;
                    taskEXIT_CRITICAL();
                    g_bt_alarm_dirty = 1;   /* 真正落盘交给 HC06_Poll() */
                }
            } else {

                if(first <= 23 && second <= 59 && third <= 59) {
                    taskENTER_CRITICAL();

                    if(!(first == 13 && second == 10)) {
                        g_bt_time.hour = first;
                        g_bt_time.minute = second;
                        g_bt_time.second = third;
                        g_bt_time_valid = 1;
                        g_bt_time_updated = 1;
                    }
                    taskEXIT_CRITICAL();
                }
            }
            break;

        default:
            stage = 0;
            break;
    }
}

/* --------------------------------------------------------------------------
 * 任务上下文的蓝牙消费入口。
 * USART1 中断只把字节推进 BSP_UART 的环形缓冲，这里负责：
 *   1) 把缓冲里的字节取出交给 HC06_ProcessByte() 解析三字节协议；
 *   2) 把解析过程中置起的闹钟脏标志落盘。
 * 由此 Flash 擦写彻底离开中断上下文，ISR 里也不再出现临界区调用。
 * -------------------------------------------------------------------------- */
void HC06_Poll(void)
{
    int b;

    while((b = BSP_UART_GetByte(BSP_UART_BT)) >= 0)
    {
        HC06_ProcessByte((uint8_t)b);
    }

    if(g_bt_alarm_dirty)
    {
        g_bt_alarm_dirty = 0;
        HC06_SaveAlarms();
    }
}

/* 读取最新的蓝牙同步时间。 */
uint8_t HC06_GetTime(HC06_Time_t *time)
{
    taskENTER_CRITICAL();
    if(g_bt_time_valid) {
        time->hour = g_bt_time.hour;
        time->minute = g_bt_time.minute;
        time->second = g_bt_time.second;
        g_bt_time_updated = 0;
        taskEXIT_CRITICAL();
        return 1;
    }
    taskEXIT_CRITICAL();
    return 0;
}

/* 查询是否收到新的时间数据。 */
uint8_t HC06_HasNewTime(void)
{
    uint8_t has_new;
    taskENTER_CRITICAL();
    has_new = g_bt_time_updated;
    if(has_new) {
        g_bt_time_updated = 0;
    }
    taskEXIT_CRITICAL();
    return has_new;
}

/* 清除时间更新标志。 */
void HC06_ClearTimeFlag(void)
{
    taskENTER_CRITICAL();
    g_bt_time_updated = 0;
    taskEXIT_CRITICAL();
}

/* 查询是否收到新的闹钟配置。 */
uint8_t HC06_HasNewAlarm(void)
{
    uint8_t has_new;
    taskENTER_CRITICAL();
    has_new = g_bt_alarm_updated;
    taskEXIT_CRITICAL();
    return has_new;
}

/* 读取最新闹钟，并清除更新标志。 */
uint8_t HC06_GetNewAlarm(uint8_t *index, HC06_Alarm_t *alarm)
{
    taskENTER_CRITICAL();
    if(g_bt_alarm_updated) {
        *index = g_bt_alarm_index;
        alarm->hour = g_bt_alarms[*index].hour;
        alarm->minute = g_bt_alarms[*index].minute;
        alarm->enabled = g_bt_alarms[*index].enabled;
        g_bt_alarm_updated = 0;
        taskEXIT_CRITICAL();
        return 1;
    }
    taskEXIT_CRITICAL();
    return 0;
}

/* 设置一组闹钟，并标记配置已更新。 */
void HC06_SetAlarm(uint8_t index, uint8_t hour, uint8_t minute, uint8_t enabled)
{
    if(index >= 3) return;
    taskENTER_CRITICAL();
    g_bt_alarms[index].hour = hour;
    g_bt_alarms[index].minute = minute;
    g_bt_alarms[index].enabled = enabled;
    g_bt_alarm_updated = 1;
    g_bt_alarm_index = index;
    taskEXIT_CRITICAL();
    g_bt_alarm_dirty = 1;   /* 真正落盘交给 HC06_Poll() */
}

/* 初始化蓝牙接收状态和默认闹钟配置。 */
void HC06_Init(void)
{

    g_bt_time_valid = 0;
    g_bt_time_updated = 0;
    g_bt_rx_count = 0;
    g_bt_debug_ready = 0;


    g_bt_alarm_updated = 0;
    g_bt_alarm_index = 0;


    g_bt_alarms[0].hour = ALARM_HOUR_1;
    g_bt_alarms[0].minute = ALARM_MINUTE_1;
    g_bt_alarms[0].enabled = 1;

    g_bt_alarms[1].hour = ALARM_HOUR_2;
    g_bt_alarms[1].minute = ALARM_MINUTE_2;
    g_bt_alarms[1].enabled = 1;

    g_bt_alarms[2].hour = ALARM_HOUR_3;
    g_bt_alarms[2].minute = ALARM_MINUTE_3;
    g_bt_alarms[2].enabled = 1;

    HC06_LoadAlarms();
}
