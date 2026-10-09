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
// 这里恢复成工程原本的取值（也就是 FreeRTOS 里最常见的 512/256 字），
// 不再按调用图逐个抠。任务走 xTaskCreate() 动态创建，
// 栈和 TCB 都从 configTOTAL_HEAP_SIZE(20KB) 里分配。
//
// 参考：armlink --callgraph 生成的静态调用图（Objects/Poject.htm）实测的
// Max Depth（已含 Cortex-M3 每次切换固定占用的 64 字节上下文帧）——
//   Upload_Task 272 B / Display_Task 428 B / Time_Task 228 B / HX711_Task 220 B
//   Key_Task 220 B / DHT11_Task 212 B / Voice_Task 212 B / AppTaskCreate 196 B
//   Servo_Task 164 B / 空闲任务 164 B
// 下面每个取值都是它的 2~4 倍，余量充足。
//
// 一个仍然要记住的坑：调用图给 printf 系列标的是 "Unknown Stack Size"，
// 走函数指针的浮点格式化（_printf_fp_dec_real 自身 104B、整条链 324B）
// 不会被计入调用者的 Max Depth。本工程只有 ProcessUploadQueue() 用了
// "%.1f"（见 app_tasks.c 里几处 sprintf），所以在 Upload_Task 上留了 1KB。
//
// 上板后 vApplicationIdleHook() 会在启动 31 秒时通过串口打印各任务的实际
// 剩余栈水位（[STACK-WATERMARK] free bytes per task:），如果某个任务 free
// 值很小就把它对应的宏调大。
#define HX711_TASK_STACK_WORDS        512   /* 2 KB */
#define DISPLAY_TASK_STACK_WORDS      512   /* 2 KB */
#define DHT11_TASK_STACK_WORDS        512   /* 2 KB */
#define UPLOAD_TASK_STACK_WORDS       256   /* 1 KB */
#define KEY_TASK_STACK_WORDS          256   /* 1 KB */
#define TIME_TASK_STACK_WORDS         512   /* 2 KB */
#define SERVO_TASK_STACK_WORDS        256   /* 1 KB */
#define VOICE_TASK_STACK_WORDS        256   /* 1 KB */
#define APPTASKCREATE_STACK_WORDS     256   /* 1 KB */

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

// 任务句柄（栈与 TCB 由内核从 20KB 堆里动态分配，这里不再有静态缓冲的 extern）
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
