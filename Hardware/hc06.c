#include "hc06.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include <string.h>
#include <stdio.h>
#include "app_tasks.h"
#include "flash_storage.h"

// ==================== 全局变量定义 ====================

/**
  * @brief  从蓝牙接收到的最新时间
  */
volatile HC06_Time_t g_bt_time = {0};

/**
  * @brief  蓝牙时间是否有效标志
  * @note   1=有效, 0=无效
  */
volatile uint8_t g_bt_time_valid = 0;

/**
  * @brief  蓝牙时间是否更新标志
  * @note   1=有新数据, 0=无新数据
  */
volatile uint8_t g_bt_time_updated = 0;

/**
  * @brief  蓝牙调试缓冲区
  * @note   存储最后3个接收到的字节，用于调试
  */
volatile uint8_t g_bt_debug_buf[3] = {0};

/**
  * @brief  调试数据就绪标志
  * @note   1=有新数据, 0=无新数据
  */
volatile uint8_t g_bt_debug_ready = 0;

/**
  * @brief  蓝牙接收字节计数
  * @note   累计统计接收到的字节数
  */
volatile uint8_t g_bt_rx_count = 0;

/**
  * @brief  3个蓝牙闹钟设置
  * @note   索引0-2对应闹钟1-3
  */
volatile HC06_Alarm_t g_bt_alarms[3] = {
    {ALARM_HOUR_1, ALARM_MINUTE_1, 1},    // 闹钟1默认值
    {ALARM_HOUR_2, ALARM_MINUTE_2, 1},    // 闹钟2默认值
    {ALARM_HOUR_3, ALARM_MINUTE_3, 1}     // 闹钟3默认值
};

/**
  * @brief  闹钟是否更新标志
  * @note   1=有新数据, 0=无新数据
  */
volatile uint8_t g_bt_alarm_updated = 0;

/**
  * @brief  最新更新的闹钟索引
  * @note   0-2对应闹钟1-3
  */
volatile uint8_t g_bt_alarm_index = 0;

// ==================== 核心函数实现 ====================

/**
  * @brief  处理接收到的单个字节数据
  * @param  byte: 接收到的字节
  * @retval 无
  * @note   通过状态机解析3字节协议：
  *         - 状态0：接收第1字节
  *         - 状态1：接收第2字节
  *         - 状态2：接收第3字节并处理
  * 
  *         协议格式：
  *         - 时间同步：第1字节0-23（小时）, 第2字节0-59（分钟）, 第3字节0-59（秒）
  *         - 闹钟设置：第1字节24-26（闹钟1-3）, 第2字节0-23（小时）, 第3字节0-59（分钟）
  * 
  *         超时机制：超过500ms未接收到下一字节则重置状态机
  *         特殊处理：过滤13:10开头的错误时间（蓝牙断开时的默认值）
  */
void HC06_ProcessByte(uint8_t byte)
{
    static uint8_t stage = 0;              // 接收状态机状态 (0-2)
    static uint8_t temp_buf[3];            // 临时缓冲区，存储3个字节
    static uint32_t last_byte_tick = 0;    // 上一字节的接收时间
    uint32_t now = xTaskGetTickCount();    // 当前时间
    
    uint8_t first, second, third;          // 三个字节的临时变量
    uint8_t alarm_index;                   // 闹钟索引
    
    // 超时检测：超过500ms未接收到下一字节则重置状态机
    if((now - last_byte_tick) > pdMS_TO_TICKS(500) && stage != 0) {
        stage = 0;
    }
    last_byte_tick = now;                  // 更新上一字节时间
    
    // 更新调试缓冲区和计数
    g_bt_debug_buf[g_bt_rx_count % 3] = byte;
    g_bt_rx_count++;
    
    // 状态机处理
    switch(stage) {
        case 0:  // 接收第1字节
            temp_buf[0] = byte;
            stage = 1;
            break;
            
        case 1:  // 接收第2字节
            temp_buf[1] = byte;
            stage = 2;
            break;
            
        case 2:  // 接收第3字节并处理
            temp_buf[2] = byte;
            stage = 0;  // 重置状态机
            
            first = temp_buf[0];
            second = temp_buf[1];
            third = temp_buf[2];
            
            // 更新调试缓冲区
            g_bt_debug_buf[0] = first;
            g_bt_debug_buf[1] = second;
            g_bt_debug_buf[2] = third;
            g_bt_debug_ready = 1;
            
            // 判断是闹钟设置还是时间同步
            if(first >= 24) {
                // 闹钟设置：第1字节24-26对应闹钟1-3
                alarm_index = first - 24;
                
                // 验证数据有效性：闹钟索引<3，小时<=23，分钟<=59
                if(alarm_index < 3 && second <= 23 && third <= 59) {
                    taskENTER_CRITICAL();
                    g_bt_alarms[alarm_index].hour = second;
                    g_bt_alarms[alarm_index].minute = third;
                    g_bt_alarms[alarm_index].enabled = 1;
                    g_bt_alarm_updated = 1;
                    g_bt_alarm_index = alarm_index;
                    taskEXIT_CRITICAL();
                    // 自动保存到Flash
                    HC06_SaveToFlash();
                }
            } else {
                // 时间同步：第1字节0-23（小时）
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
            
        default:  // 异常状态，重置
            stage = 0;
            break;
    }
}

/**
  * @brief  获取从蓝牙同步的时间
  * @param  time: 输出参数，存储时间的指针
  * @retval 1=成功获取, 0=无有效时间
  * @note   获取后会清除更新标志，需要使用临界区保护
  */
uint8_t HC06_GetTime(HC06_Time_t *time)
{
    taskENTER_CRITICAL();
    if(g_bt_time_valid) {
        time->hour = g_bt_time.hour;
        time->minute = g_bt_time.minute;
        time->second = g_bt_time.second;
        g_bt_time_updated = 0;  // 清除更新标志
        taskEXIT_CRITICAL();
        return 1;
    }
    taskEXIT_CRITICAL();
    return 0;
}

/**
  * @brief  检查是否有新的蓝牙时间数据
  * @param  无
  * @retval 1=有新数据, 0=无新数据
  * @note   读取后不会清除标志，需调用HC06_ClearTimeFlag清除
  */
uint8_t HC06_HasNewTime(void)
{
    uint8_t has_new;
    taskENTER_CRITICAL();
    has_new = g_bt_time_updated;
    if(has_new) {
        g_bt_time_updated = 0;  // 清除标志
    }
    taskEXIT_CRITICAL();
    return has_new;
}

/**
  * @brief  清除蓝牙时间更新标志
  * @param  无
  * @retval 无
  */
void HC06_ClearTimeFlag(void)
{
    taskENTER_CRITICAL();
    g_bt_time_updated = 0;
    taskEXIT_CRITICAL();
}

/**
  * @brief  检查是否有新的闹钟数据
  * @param  无
  * @retval 1=有新数据, 0=无新数据
  */
uint8_t HC06_HasNewAlarm(void)
{
    uint8_t has_new;
    taskENTER_CRITICAL();
    has_new = g_bt_alarm_updated;
    taskEXIT_CRITICAL();
    return has_new;
}

/**
  * @brief  获取最新更新的闹钟数据
  * @param  index: 输出参数，闹钟索引 (0-2)
  * @param  alarm: 输出参数，闹钟数据指针
  * @retval 1=成功获取, 0=无新数据
  * @note   获取后会清除更新标志
  */
uint8_t HC06_GetNewAlarm(uint8_t *index, HC06_Alarm_t *alarm)
{
    taskENTER_CRITICAL();
    if(g_bt_alarm_updated) {
        *index = g_bt_alarm_index;
        alarm->hour = g_bt_alarms[*index].hour;
        alarm->minute = g_bt_alarms[*index].minute;
        alarm->enabled = g_bt_alarms[*index].enabled;
        g_bt_alarm_updated = 0;  // 清除更新标志
        taskEXIT_CRITICAL();
        return 1;
    }
    taskEXIT_CRITICAL();
    return 0;
}

/**
  * @brief  设置指定闹钟的时间
  * @param  index: 闹钟索引 (0-2)
  * @param  hour: 闹钟小时 (0-23)
  * @param  minute: 闹钟分钟 (0-59)
  * @param  enabled: 闹钟使能标志
  * @retval 无
  * @note   会设置更新标志，需要使用临界区保护
  */
void HC06_SetAlarm(uint8_t index, uint8_t hour, uint8_t minute, uint8_t enabled)
{
    if(index >= 3) return;  // 参数校验
    taskENTER_CRITICAL();
    g_bt_alarms[index].hour = hour;
    g_bt_alarms[index].minute = minute;
    g_bt_alarms[index].enabled = enabled;
    g_bt_alarm_updated = 1;
    g_bt_alarm_index = index;
    taskEXIT_CRITICAL();
    
    // 自动保存到Flash
    HC06_SaveToFlash();
}

/**
  * @brief  保存所有闹钟数据到Flash
  * @param  无
  * @retval 1=保存成功, 0=保存失败
  */
uint8_t HC06_SaveToFlash(void)
{
    FlashAlarm_t tempAlarms[3];
    
    taskENTER_CRITICAL();
    // 复制数据到临时缓冲区
    for(int i = 0; i < 3; i++)
    {
        tempAlarms[i].hour = g_bt_alarms[i].hour;
        tempAlarms[i].minute = g_bt_alarms[i].minute;
        tempAlarms[i].enabled = g_bt_alarms[i].enabled;
    }
    taskEXIT_CRITICAL();
    
    // 保存到Flash
    return FlashStorage_SaveAlarms(tempAlarms);
}

/**
  * @brief  从Flash加载所有闹钟数据
  * @param  无
  * @retval 1=加载成功, 0=加载失败
  */
uint8_t HC06_LoadFromFlash(void)
{
    FlashAlarm_t tempAlarms[3];
    
    if(FlashStorage_LoadAlarms(tempAlarms))
    {
        taskENTER_CRITICAL();
        for(int i = 0; i < 3; i++)
        {
            g_bt_alarms[i].hour = tempAlarms[i].hour;
            g_bt_alarms[i].minute = tempAlarms[i].minute;
            g_bt_alarms[i].enabled = tempAlarms[i].enabled;
        }
        taskEXIT_CRITICAL();
        return 1;
    }
    return 0;
}

/**
  * @brief  HC06蓝牙模块初始化
  * @param  无
  * @retval 无
  * @note   初始化所有全局变量和默认闹钟设置
  */
void HC06_Init(void)
{
    // 初始化Flash存储模块
    FlashStorage_Init();
    
    // 初始化时间相关变量
    g_bt_time_valid = 0;
    g_bt_time_updated = 0;
    g_bt_rx_count = 0;
    g_bt_debug_ready = 0;
    
    // 初始化闹钟相关变量
    g_bt_alarm_updated = 0;
    g_bt_alarm_index = 0;
    
    // 先尝试从Flash加载数据
    if(!HC06_LoadFromFlash())
    {
        // 如果Flash中没有有效数据，使用默认值
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
}
