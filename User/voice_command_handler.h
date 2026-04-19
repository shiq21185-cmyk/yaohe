#ifndef __VOICE_COMMAND_HANDLER_H
#define __VOICE_COMMAND_HANDLER_H

#include "stm32f10x.h"
#include <stdint.h>

// 全局变量声明
extern volatile uint8_t g_voice_alarm_edit;          // 0=未在修改, 1=等待输入闹钟序号, 2=等待输入小时, 3=等待输入分钟
extern volatile uint8_t g_voice_alarm_index;         // 选中的闹钟序号 (1-3)
extern volatile uint8_t g_voice_alarm_hour;          // 输入的小时
extern volatile uint8_t g_voice_alarm_minute;        // 输入的分钟

// 函数声明
void UpdateAlarmTime(uint8_t index, uint8_t hour, uint8_t minute);
void HandleVoiceCommand(uint8_t cmd_type, uint8_t cmd_id);

#endif
