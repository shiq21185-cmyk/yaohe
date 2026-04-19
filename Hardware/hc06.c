#include "hc06.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>
#include "app_tasks.h"

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
                }
            } else {
                if(first <= 23 && second <= 59 && third <= 59) {
                    taskENTER_CRITICAL();
                    // 过滤蓝牙断开时的所有13:10开头的错误时间
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

void HC06_ClearTimeFlag(void)
{
    taskENTER_CRITICAL();
    g_bt_time_updated = 0;
    taskEXIT_CRITICAL();
}

uint8_t HC06_HasNewAlarm(void)
{
    uint8_t has_new;
    taskENTER_CRITICAL();
    has_new = g_bt_alarm_updated;
    taskEXIT_CRITICAL();
    return has_new;
}

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
}

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
}
