#ifndef __HC06_H
#define __HC06_H

#include "stm32f10x.h"

/* 蓝牙同步得到的时、分、秒。 */
typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t second;
} HC06_Time_t;

/* 三组闹钟共用的数据结构。 */
typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t enabled;
} HC06_Alarm_t;

extern volatile HC06_Time_t g_bt_time;
extern volatile uint8_t g_bt_time_valid;
extern volatile uint8_t g_bt_time_updated;
extern volatile uint8_t g_bt_debug_buf[3];
extern volatile uint8_t g_bt_debug_ready;
extern volatile uint8_t g_bt_rx_count;
extern volatile HC06_Alarm_t g_bt_alarms[3];
extern volatile uint8_t g_bt_alarm_updated;
extern volatile uint8_t g_bt_alarm_index;

void HC06_Init(void);
/* 任务上下文的蓝牙消费入口：取环形缓冲里的字节解析协议，并落地闹钟脏标志。
 * 需要在某个周期任务里被反复调用（当前挂在 Time_Task）。 */
void HC06_Poll(void);
void HC06_ProcessByte(uint8_t byte);
uint8_t HC06_GetTime(HC06_Time_t *time);
uint8_t HC06_HasNewTime(void);
void HC06_ClearTimeFlag(void);
uint8_t HC06_HasNewAlarm(void);
uint8_t HC06_GetNewAlarm(uint8_t *index, HC06_Alarm_t *alarm);
void HC06_SetAlarm(uint8_t index, uint8_t hour, uint8_t minute, uint8_t enabled);
void HC06_SaveAlarms(void);

#endif
