#ifndef __HC06_H
#define __HC06_H

#include "stm32f10x.h"

// ==================== 数据结构定义 ====================

/**
  * @brief  时间数据结构
  * @note   用于存储从蓝牙接收到的系统时间
  */
typedef struct {
    uint8_t hour;      // 小时 (0-23)
    uint8_t minute;    // 分钟 (0-59)
    uint8_t second;    // 秒 (0-59)
} HC06_Time_t;

/**
  * @brief  闹钟数据结构
  * @note   用于存储闹钟的时间设置
  */
typedef struct {
    uint8_t hour;      // 闹钟小时 (0-23)
    uint8_t minute;    // 闹钟分钟 (0-59)
    uint8_t enabled;   // 闹钟使能标志 (1=启用, 0=禁用)
} HC06_Alarm_t;

// ==================== 全局变量声明 ====================

extern volatile HC06_Time_t g_bt_time;         // 从蓝牙接收到的最新时间
extern volatile uint8_t g_bt_time_valid;       // 蓝牙时间是否有效标志
extern volatile uint8_t g_bt_time_updated;     // 蓝牙时间是否更新标志
extern volatile uint8_t g_bt_debug_buf[3];     // 蓝牙调试缓冲区，存储最后3个字节
extern volatile uint8_t g_bt_debug_ready;      // 调试数据就绪标志
extern volatile uint8_t g_bt_rx_count;         // 蓝牙接收字节计数

extern volatile HC06_Alarm_t g_bt_alarms[3];   // 3个蓝牙闹钟设置
extern volatile uint8_t g_bt_alarm_updated;    // 闹钟是否更新标志
extern volatile uint8_t g_bt_alarm_index;      // 最新更新的闹钟索引

// ==================== 函数声明 ====================

/**
  * @brief  HC06蓝牙模块初始化
  * @param  无
  * @retval 无
  * @note   初始化所有全局变量和默认闹钟设置
  */
void HC06_Init(void);

/**
  * @brief  处理接收到的单个字节数据
  * @param  byte: 接收到的字节
  * @retval 无
  * @note   通过状态机解析3字节协议，超时时间500ms
  *         协议格式：
  *         - 时间同步：0-23（小时）, 0-59（分钟）, 0-59（秒）
  *         - 闹钟设置：24-26（闹钟1-3）, 0-23（小时）, 0-59（分钟）
  */
void HC06_ProcessByte(uint8_t byte);

/**
  * @brief  获取从蓝牙同步的时间
  * @param  time: 输出参数，存储时间的指针
  * @retval 1=成功获取, 0=无有效时间
  * @note   获取后会清除更新标志
  */
uint8_t HC06_GetTime(HC06_Time_t *time);

/**
  * @brief  检查是否有新的蓝牙时间数据
  * @param  无
  * @retval 1=有新数据, 0=无新数据
  * @note   读取后不会清除标志，需调用HC06_ClearTimeFlag清除
  */
uint8_t HC06_HasNewTime(void);

/**
  * @brief  清除蓝牙时间更新标志
  * @param  无
  * @retval 无
  */
void HC06_ClearTimeFlag(void);

/**
  * @brief  检查是否有新的闹钟数据
  * @param  无
  * @retval 1=有新数据, 0=无新数据
  */
uint8_t HC06_HasNewAlarm(void);

/**
  * @brief  获取最新更新的闹钟数据
  * @param  index: 输出参数，闹钟索引 (0-2)
  * @param  alarm: 输出参数，闹钟数据指针
  * @retval 1=成功获取, 0=无新数据
  * @note   获取后会清除更新标志
  */
uint8_t HC06_GetNewAlarm(uint8_t *index, HC06_Alarm_t *alarm);

/**
  * @brief  设置指定闹钟的时间
  * @param  index: 闹钟索引 (0-2)
  * @param  hour: 闹钟小时 (0-23)
  * @param  minute: 闹钟分钟 (0-59)
  * @param  enabled: 闹钟使能标志
  * @retval 无
  * @note   会设置更新标志
  */
void HC06_SetAlarm(uint8_t index, uint8_t hour, uint8_t minute, uint8_t enabled);

/**
  * @brief  保存所有闹钟数据到Flash
  * @param  无
  * @retval 1=保存成功, 0=保存失败
  */
uint8_t HC06_SaveToFlash(void);

/**
  * @brief  从Flash加载所有闹钟数据
  * @param  无
  * @retval 1=加载成功, 0=加载失败
  */
uint8_t HC06_LoadFromFlash(void);

#endif
