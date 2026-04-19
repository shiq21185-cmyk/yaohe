#ifndef __XR_VOICE_H
#define __XR_VOICE_H

#include "stm32f10x.h"
#include <stdint.h>
#include "FreeRTOS.h"  
#include "task.h" 
#include "semphr.h"    

// ???????
#define XRVOICE_RX_BUFFER_SIZE  64

// ==================== 命令类型定义 ====================
#define XRVOICE_WAKEUP          0x01    // 唤醒
#define XRVOICE_OPEN_LID        0x02    // 打开盖孝
#define XRVOICE_CLOSE_LID       0x03    // 关闭盖孝
#define XRVOICE_LEFT            0x04    // 左转
#define XRVOICE_RIGHT           0x05    // 坳转
#define XRVOICE_STOP            0x06    // 坜止
#define XRVOICE_LIGHT_ON        0x07    // 开睯
#define XRVOICE_LIGHT_OFF       0x08    // 关睯
#define XRVOICE_SLEEP           0x09    // 休眠
#define XRVOICE_LEFT_SHIFT      0x0A    // 左平移
#define XRVOICE_RIGHT_SHIFT     0x0B    // 坳平移
#define XRVOICE_CALL_GUARDIAN   0x0C    // 蝔系监护人
#define XRVOICE_VOL_UP          0x0D    // 增大音針
#define XRVOICE_VOL_DOWN        0x0E    // 凝尝音針
#define XRVOICE_VOL_MAX         0x0F    // 最大音針
#define XRVOICE_VOL_MID         0x10    // 中等音針
#define XRVOICE_VOL_MIN         0x11    // 最尝音針
#define XRVOICE_ENABLE_REPORT   0x12    // 开坯播报
#define XRVOICE_DISABLE_REPORT  0x13    // 关闭播报
#define XRVOICE_PASSIVE_REPORT  0x14    // 被动上报
#define XRVOICE_QUERY_TIME      0x15    // 查询时间

// 信坷針声明
extern SemaphoreHandle_t xVoiceSemaphore;

// 回调函数类型定义
typedef void (*VoiceCommandCallback_t)(uint8_t command, uint8_t param);

// 初始化语音模块
void XRVoice_Init(VoiceCommandCallback_t callback);

// 语音处睆任务（在RTOS任务中循环调用）
void XRVoice_Task(void);

// 获坖唤醒状思
uint8_t XRVoice_IsWakeup(void);

// 手动唤醒
void XRVoice_Wakeup(void);

// 播放语音（1-9对应丝坌语音）
void XRVoice_Play(uint8_t index);

// 播放原始命令（统一格弝 AA 55 cmd_type cmd_id FB）
// cmd_type: 命令类型（最大 0x09）
// cmd_id: 命令ID（最大 0x00, 0x01, 0x02）
void XRVoice_PlayRaw(uint8_t cmd_type, uint8_t cmd_id);
//音針控制
void XRVoice_VolumeUp(void);
void XRVoice_VolumeDown(void);
void XRVoice_VolumeMax(void);
void XRVoice_VolumeMid(void);
void XRVoice_VolumeMin(void);
// 设置音針 (1-6)
void XRVoice_SetVolume(uint8_t volume);

// 坜止播放
void XRVoice_Stop(void);

#endif
