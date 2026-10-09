#ifndef __APP_TASKS_H
#define __APP_TASKS_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "OLED.h"
#include "hx711.h"
#include "dht11.h"
#include "esp8266.h"
#include "xrvoice.h"
#include "hc06.h"
#include "key.h"
// MQTT主题定义
#define TOPIC_TEMP "stm32/temperature"
#define TOPIC_HUMID "stm32/humidity"
#define TOPIC_WEIGHT "stm32/weight"
#define TOPIC_STATUS "stm32/status"

#define ALARM_HOUR_1    7
#define ALARM_MINUTE_1  30
#define ALARM_HOUR_2    7
#define ALARM_MINUTE_2  31
#define ALARM_HOUR_3    7
#define ALARM_MINUTE_3  32

// ==================== 舵机参数 ====================
#define SERVO_ANGLE_OPEN        45     // 打开角度，药盒开启状态
#define SERVO_ANGLE_CLOSE       135      // 关闭角度，药盒关闭状态

// 舵机命令
typedef enum {
    SERVO_CMD_NONE = 0,
    SERVO_CMD_OPEN,         // 打开
    SERVO_CMD_CLOSE,        // 关闭
    SERVO_CMD_STOP          // 停止当前动作
} Servo_Command_t;

// ==================== 任务栈深（单位：字，1字=4字节） ====================
// 取值依据是 armlink --callgraph 生成的静态调用图（Objects/Poject.htm）
// 里各任务函数的 "Max Depth"，加上 Cortex-M3 每次任务切换固定占用的
// 64 字节上下文帧（8字异常帧 + 8字 r4-r11），再留约 1.5~1.8 倍余量。
//
// 两个必须记住的坑：
//  1) 调用图给 printf 系列标的是 "Unknown Stack Size"，走函数指针的浮点
//     格式化（_printf_fp_dec_real 自身 104B、整条链 324B）不会被计入调用者
//     的 Max Depth。本工程只有 ProcessUploadQueue() 用了 "%.1f"（见
//     app_tasks.c 里几处 sprintf），所以只有 Upload_Task 要额外为它留空间，
//     这里直接给 512B。
//  2) ESP_ConnectMQTT()/ESP_MQTTPublish() 原先各自在栈上开 char cmd[256]，
//     改成共用的文件级静态缓冲（Hardware/esp8266.c 的 esp_cmd_buf）之后，
//     main 的最大深度从 408B 掉到 172B，Upload_Task 从 456B 掉到 208B，
//     栈深才敢跟着往下砍。
//
//   任务            调用图MaxDepth  +64B帧    本次取值
//   Upload_Task        208 B       272 B     512 B (128字，另含浮点printf余量)
//   Display_Task       364 B       428 B     640 B (160字)
//   Time_Task          164 B       228 B     384 B  (96字)
//   HX711_Task         156 B       220 B     384 B  (96字)
//   Key_Task           156 B       220 B     384 B  (96字)
//   DHT11_Task         148 B       212 B     384 B  (96字)
//   Voice_Task         148 B       212 B     384 B  (96字)
//   AppTaskCreate      132 B       196 B     384 B  (96字)
//   Servo_Task         100 B       164 B     256 B  (64字)
//   空闲任务           100 B       164 B     256 B (见 configMINIMAL_STACK_SIZE)
//
// 上板后 vApplicationIdleHook() 会在启动 31 秒时通过串口打印各任务的实际
// 剩余栈水位（[STACK-WATERMARK] free bytes per task:），如果某个任务 free
// 值很小（比如不足 64 字节）就把它对应的宏调大。
#define HX711_TASK_STACK_WORDS         96   /* 2048 ->  384 B */
#define DISPLAY_TASK_STACK_WORDS      160   /* 2048 ->  640 B */
#define DHT11_TASK_STACK_WORDS         96   /* 2048 ->  384 B */
#define UPLOAD_TASK_STACK_WORDS       128   /* 1024 ->  512 B */
#define KEY_TASK_STACK_WORDS           96   /* 1024 ->  384 B */
#define TIME_TASK_STACK_WORDS          96   /* 2048 ->  384 B */
#define SERVO_TASK_STACK_WORDS         64   /* 1024 ->  256 B */
#define VOICE_TASK_STACK_WORDS         96   /* 1024 ->  384 B */
#define APPTASKCREATE_STACK_WORDS      96   /* 1024 ->  384 B */

// 外部变量
extern volatile uint8_t g_servo_cmd;           // 当前舵机命令
extern volatile uint8_t g_servo_state;       // 0=关闭, 1=打开
extern TaskHandle_t Servo_Task_Handle;

// 舵机控制函数
void Servo_SendCommand(Servo_Command_t cmd);
void Servo_Open(void);      // 打开药盒
void Servo_Close(void);     // 关闭药盒

// 屏幕3编辑模式状态
#define EDIT_MODE_NONE    0   // 非编辑模式
#define EDIT_MODE_ALARM1  1   // 编辑闹钟1
#define EDIT_MODE_ALARM2  2   // 编辑闹钟2
#define EDIT_MODE_ALARM3  3   // 编辑闹钟3
#define EDIT_MODE_VOLUME  4   // 编辑音量

extern volatile uint8_t g_upload_status;
extern volatile uint32_t g_upload_display_tick;
extern volatile uint8_t g_need_upload;
extern volatile uint8_t display_mode;
extern float temperature;
extern float humidity;
extern s32 weight;
extern uint8_t tare_done;
extern uint8_t dht11_last_status;
extern volatile uint8_t current_hour;
extern volatile uint8_t current_minute;
extern volatile uint8_t current_second;
extern volatile uint8_t g_key1_pressed;
extern volatile uint8_t g_last_hex[3];
extern volatile uint8_t g_hex_received;
extern volatile uint32_t g_last_sync_tick;
extern volatile uint8_t g_sync_status;
extern SemaphoreHandle_t xTimeMutex;

// 屏幕3编辑相关变量
extern volatile uint8_t g_edit_mode;      // 当前编辑模式
extern volatile uint8_t g_edit_field;     // 0=小时, 1=分钟
extern volatile uint8_t g_volume;         // 当前音量 1-5

extern StackType_t Idle_Task_Stack[configMINIMAL_STACK_SIZE];
extern StaticTask_t Idle_Task_TCB;
#if ( configUSE_TIMERS == 1 )
extern StackType_t Timer_Task_Stack[configTIMER_TASK_STACK_DEPTH];
extern StaticTask_t Timer_Task_TCB;
#endif

// 任务创建任务自身的静态内存（在 main.c 里用 xTaskCreateStatic 创建）
extern StackType_t AppTaskCreate_Stack[];
extern StaticTask_t AppTaskCreate_TCB;

extern TaskHandle_t AppTaskCreate_Handle;
extern TaskHandle_t HX711_Task_Handle;
extern TaskHandle_t DHT11_Task_Handle;
extern TaskHandle_t Display_Task_Handle;
extern TaskHandle_t Upload_Task_Handle;
extern TaskHandle_t Key_Task_Handle;
extern TaskHandle_t Time_Task_Handle;

void AppTaskCreate(void);
void HX711_Task(void* parameter);
void DHT11_Task(void* parameter);
void Display_Task(void* parameter);
void Upload_Task(void* parameter);
void Key_Task(void* parameter);
void Time_Task(void* parameter);
void Servo_Task(void* parameter);
void Voice_Task(void* parameter);

void Voice_Play(uint8_t index);
void Voice_Play_Dot(void);
void Voice_Play_Number(uint8_t num);
void Voice_Play_Number_Full(uint8_t num);
void Voice_Play_Current_Time(void);
void Voice_Play_Current_Time_With_Period(void);
uint8_t ReadDHT11Data(void);
void Fast_UploadData(uint8_t type);
void Update_Time(void);
void Get_Current_Time(uint8_t *hour, uint8_t *minute, uint8_t *second);
uint8_t Calibrate_Time(uint8_t new_hour, uint8_t new_minute, uint8_t new_second, uint8_t force);
uint8_t Is_Sync_Valid(void);
uint8_t Check_Alarm_Time(void);
void UpdateDisplay(void);
void CheckAndTriggerUpload(void);
void CheckAndTriggerUpload_Weight(void);
void CheckAndTriggerUpload_Status(void);
void ProcessUploadQueue(void);

void StackWatermarkReport(void);

extern volatile HC06_Alarm_t g_bt_alarms[3];
extern volatile uint8_t g_bt_alarm_updated;

#endif
