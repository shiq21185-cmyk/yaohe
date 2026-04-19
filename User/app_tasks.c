#include "app_tasks.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "OLED.h"
#include "light.h"
#include "LED.h"
#include "Servo.h"
#include "PWM.h"
#include "hc06.h"
#include "voice_command_handler.h"

// ==================== 全局变量和宏定义 ====================
#define WEIGHT_UPLOAD_THRESHOLD 0.8f    // 重量变化阈值0.8g，变化接近1g就立即上传，响应更快
#define STATUS_UPLOAD_THRESHOLD 0       // 状态码只要有变化就上传
#define UPLOAD_DEBOUNCE_MS      370     // 同一数据最小上传间隔
// 上传类型定义
#define UPLOAD_TYPE_NONE        0
#define UPLOAD_TYPE_TEMP        1
#define UPLOAD_TYPE_HUMID       2
#define UPLOAD_TYPE_WEIGHT      3
#define UPLOAD_TYPE_STATUS      4
// 状态码定义（独立状态）
#define BOX_CLOSED        0   // 盒子关闭
#define BOX_OPEN          1   // 盒子打开
#define ALARM_RINGING     2   // 闹钟响铃
#define ALARM_SILENT      3   // 闹钟不响
#define BOX_EMPTY         4   // 药盒为空
#define BOX_NOT_EMPTY     5   // 药盒不空

// 状态变量
volatile uint8_t g_box_state = BOX_CLOSED;        // 盒子状态
volatile uint8_t g_alarm_state = ALARM_SILENT;    // 闹钟状态
volatile uint8_t g_box_content_state = BOX_NOT_EMPTY; // 药盒内容状态

// 上次上传的状态
volatile uint8_t g_last_box_state = BOX_CLOSED;
volatile uint8_t g_last_alarm_state = ALARM_SILENT;
volatile uint8_t g_last_box_content_state = BOX_NOT_EMPTY;
volatile float last_upload_weight = -9999.0f;        // 上次上传的重量

// 上传队列（简化版，用标志位）
volatile uint8_t g_upload_trigger_weight = 0;
volatile uint8_t g_upload_trigger_status = 0;
volatile uint8_t g_upload_trigger_temp = 0;
volatile uint8_t g_upload_trigger_humid = 0;

// 独立状态上传触发标志
volatile uint8_t g_upload_trigger_box = 0;
volatile uint8_t g_upload_trigger_alarm = 0;
volatile uint8_t g_upload_trigger_box_content = 0;
#define SYNC_VALID_DURATION_MS  0xFFFFFFFF   //蓝牙同步永久有效，断开蓝牙时间不会重置
#define TIME_TASK_DELAY_MS    245     // Time_Task执行周期（毫秒），等于1秒。
// ==================== 舵机 ====================
volatile uint8_t g_servo_cmd = SERVO_CMD_NONE;
volatile uint8_t g_servo_state = 0;        // 0=关闭, 1=打开
TaskHandle_t Servo_Task_Handle = NULL;

volatile uint8_t g_upload_status = 0;
volatile uint32_t g_upload_display_tick = 0;
volatile uint8_t g_need_upload = 0;
// 闹钟持续提醒相关变量
volatile uint8_t g_alarm_ringing = 0;         // 闹钟正在响
volatile uint8_t g_light_changed = 0;         // 光敏变化标志
volatile uint32_t g_alarm_start_tick = 0;     // 闹钟开始时间
volatile uint32_t g_last_alarm_play_tick = 0; // 上次播放时间
volatile uint8_t g_last_light_state = 0;      // 上次光敏状态
volatile uint8_t g_alarm_servo_state = 0;     // 闹钟舵机状态：0=无闹钟/结束，1=响铃等待开盖，2=已开盖等待关盖

volatile uint8_t display_mode = 0;

float temperature = 0;
float humidity = 0;
float last_upload_temp = -999.0f;
float last_upload_humid = -999.0f;

s32 weight = 0;
uint8_t tare_done = 0;

uint8_t dht11_last_status = 0;

volatile uint8_t g_stop_empty_box_alarm = 0;   // 停止空盒报警标志
volatile uint8_t g_empty_box_alarm_active = 0; // 空盒报警激活标志

volatile uint8_t current_hour = 8;
volatile uint8_t current_minute = 30;
volatile uint8_t current_second = 0;

volatile uint8_t g_key1_pressed = 0;
uint8_t show_bt_time = 0;
volatile uint8_t g_last_hex[3] = {0};
volatile uint8_t g_hex_received = 0;

volatile uint32_t g_last_sync_tick = 0;
volatile uint8_t  g_sync_status = 0;
volatile uint32_t g_time_task_run_count = 0;

// 屏幕3编辑相关变量
volatile uint8_t g_edit_mode = EDIT_MODE_NONE;   // 当前编辑模式
volatile uint8_t g_edit_field = 0;                // 0=小时, 1=分钟
volatile uint8_t g_volume = 1;                    // 当前音量 1-5



SemaphoreHandle_t xTimeMutex = NULL;

StackType_t Idle_Task_Stack[configMINIMAL_STACK_SIZE];
StackType_t Timer_Task_Stack[configTIMER_TASK_STACK_DEPTH];
StaticTask_t Idle_Task_TCB;
StaticTask_t Timer_Task_TCB;

TaskHandle_t AppTaskCreate_Handle = NULL;
TaskHandle_t HX711_Task_Handle = NULL;
TaskHandle_t DHT11_Task_Handle = NULL;
TaskHandle_t Display_Task_Handle = NULL;
TaskHandle_t Upload_Task_Handle = NULL;
TaskHandle_t Key_Task_Handle = NULL;
TaskHandle_t Time_Task_Handle = NULL;
TaskHandle_t Voice_Task_Handle = NULL;

#define TEMP_UPLOAD_THRESHOLD   0.2f
#define HUMID_UPLOAD_THRESHOLD  1.0f
#define UPLOAD_MIN_INTERVAL     500

void vApplicationGetIdleTaskMemory(StaticTask_t **ppxIdleTaskTCBBuffer,
                                   StackType_t **ppxIdleTaskStackBuffer,
                                   uint32_t *pulIdleTaskStackSize)
{
    *ppxIdleTaskTCBBuffer = &Idle_Task_TCB;
    *ppxIdleTaskStackBuffer = Idle_Task_Stack;
    *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
}

void vApplicationGetTimerTaskMemory(StaticTask_t **ppxTimerTaskTCBBuffer,
                                    StackType_t **ppxTimerTaskStackBuffer,
                                    uint32_t *pulTimerTaskStackSize)
{
    *ppxTimerTaskTCBBuffer = &Timer_Task_TCB;
    *ppxTimerTaskStackBuffer = Timer_Task_Stack;
    *pulTimerTaskStackSize = configTIMER_TASK_STACK_DEPTH;
}
// ==================== 检查状态码并触发上传 ====================
void CheckAndTriggerUpload_Weight(void)
{
    static TickType_t last_upload_tick = 0;
    TickType_t now = xTaskGetTickCount();
    
    // 防抖：同一数据最小间隔
    if((now - last_upload_tick) < pdMS_TO_TICKS(UPLOAD_DEBOUNCE_MS))
        return;
    
    s32 weight_diff = abs(weight - last_upload_weight);
    
    if(weight_diff >= WEIGHT_UPLOAD_THRESHOLD)
    {
        g_upload_trigger_weight = 1;
        g_upload_status = UPLOAD_TYPE_WEIGHT;  // 设置显示状态
        g_upload_display_tick = now;
        last_upload_tick = now;
    }
}

// ==================== 检查状态码并触发上传 ====================
void CheckAndTriggerUpload_Status(void)
{
    static TickType_t last_upload_tick = 0;
    TickType_t now = xTaskGetTickCount();
    
    if((now - last_upload_tick) < pdMS_TO_TICKS(UPLOAD_DEBOUNCE_MS))
        return;
    
    // 检查盒子状态
    if(g_box_state != g_last_box_state)
    {
        g_upload_trigger_box = 1;
        g_upload_status = UPLOAD_TYPE_STATUS;
        g_upload_display_tick = now;
        g_last_box_state = g_box_state;
    }
    
    // 检查闹钟状态
    if(g_alarm_state != g_last_alarm_state)
    {
        g_upload_trigger_alarm = 1;
        g_upload_status = UPLOAD_TYPE_STATUS;
        g_upload_display_tick = now;
        g_last_alarm_state = g_alarm_state;
    }
    
    // 检查药盒内容状态
    if(g_box_content_state != g_last_box_content_state)
    {
        g_upload_trigger_box_content = 1;
        g_upload_status = UPLOAD_TYPE_STATUS;
        g_upload_display_tick = now;
        g_last_box_content_state = g_box_content_state;
    }
    
    last_upload_tick = now;
}

// ==================== 统一的上传处理函数 ====================
void ProcessUploadQueue(void)
{
    char payload[32];
    
    // 优先级：状态 > 重量 > 温度 > 湿度
    
    // 盒子状态上传
    if(g_upload_trigger_box)
    {
        sprintf(payload, "%d", g_box_state);
        ESP_MQTTPublish((uint8_t*)TOPIC_STATUS, (uint8_t*)payload);
        g_upload_trigger_box = 0;
        return; // 每次只处理一个，避免阻塞
    }
    
    // 闹钟状态上传
    if(g_upload_trigger_alarm)
    {
        sprintf(payload, "%d", g_alarm_state);
        ESP_MQTTPublish((uint8_t*)TOPIC_STATUS, (uint8_t*)payload);
        g_upload_trigger_alarm = 0;
        return;
    }
    
    // 药盒内容状态上传
    if(g_upload_trigger_box_content)
    {
        sprintf(payload, "%d", g_box_content_state);
        ESP_MQTTPublish((uint8_t*)TOPIC_STATUS, (uint8_t*)payload);
        g_upload_trigger_box_content = 0;
        return;
    }
    
    if(g_upload_trigger_weight)
    {
        sprintf(payload, "%d", weight);
        ESP_MQTTPublish((uint8_t*)TOPIC_WEIGHT, (uint8_t*)payload);
        last_upload_weight = (float)weight;
        g_upload_trigger_weight = 0;
        return;
    }
    
    if(g_upload_trigger_temp)
    {
        sprintf(payload, "%.1f", temperature);
        ESP_MQTTPublish((uint8_t*)TOPIC_TEMP, (uint8_t*)payload);
        last_upload_temp = temperature;
        g_upload_trigger_temp = 0;
        return;
    }
    
    if(g_upload_trigger_humid)
    {
        sprintf(payload, "%.1f", humidity);
        ESP_MQTTPublish((uint8_t*)TOPIC_HUMID, (uint8_t*)payload);
        last_upload_humid = humidity;
        g_upload_trigger_humid = 0;
        return;
    }
}

void Voice_Play(uint8_t index)
{
    if(index < 1 || index > 9) return;
	
    XRVoice_Play(index);
}

// 播放"点"语音
void Voice_Play_Dot(void)
{
    XRVoice_PlayRaw(0x10, 0x00);
}

// 播放数字语音 (0-9)
void Voice_Play_Number(uint8_t num)
{
    if(num > 9) return;
    
    // 映射数字到语音命令
    switch(num)
    {
        case 0: XRVoice_PlayRaw(0x02, 0x08); break; // 零
        case 1: XRVoice_PlayRaw(0x02, 0x09); break; // 一
        case 2: XRVoice_PlayRaw(0x02, 0x0A); break; // 二
        case 3: XRVoice_PlayRaw(0x02, 0x0B); break; // 三
        case 4: XRVoice_PlayRaw(0x02, 0x0C); break; // 四
        case 5: XRVoice_PlayRaw(0x03, 0x00); break; // 五
        case 6: XRVoice_PlayRaw(0x03, 0x01); break; // 六
        case 7: XRVoice_PlayRaw(0x03, 0x02); break; // 七
        case 8: XRVoice_PlayRaw(0x03, 0x03); break; // 八
        case 9: XRVoice_PlayRaw(0x03, 0x04); break; // 九
    }
}

// 播放数字语音 (0-60)
void Voice_Play_Number_Full(uint8_t num)
{
    if(num == 0)
    {
        XRVoice_PlayRaw(0x02, 0x08); // 零
    }
    else if(num == 1)
    {
        XRVoice_PlayRaw(0x02, 0x09); // 一
    }
    else if(num == 2)
    {
        XRVoice_PlayRaw(0x02, 0x0A); // 二
    }
    else if(num == 3)
    {
        XRVoice_PlayRaw(0x02, 0x0B); // 三
    }
    else if(num == 4)
    {
        XRVoice_PlayRaw(0x02, 0x0C); // 四
    }
    else if(num == 5)
    {
        XRVoice_PlayRaw(0x03, 0x00); // 五
    }
    else if(num == 6)
    {
        XRVoice_PlayRaw(0x03, 0x01); // 六
    }
    else if(num == 7)
    {
        XRVoice_PlayRaw(0x03, 0x02); // 七
    }
    else if(num == 8)
    {
        XRVoice_PlayRaw(0x03, 0x03); // 八
    }
    else if(num == 9)
    {
        XRVoice_PlayRaw(0x03, 0x04); // 九
    }
    else if(num == 10)
    {
        XRVoice_PlayRaw(0x03, 0x05); // 十
    }
    else if(num == 11)
    {
        XRVoice_PlayRaw(0x03, 0x06); // 十一
    }
    else if(num == 12)
    {
        XRVoice_PlayRaw(0x03, 0x07); // 十二
    }
    else if(num == 13)
    {
        XRVoice_PlayRaw(0x03, 0x08); // 十三
    }
    else if(num == 14)
    {
        XRVoice_PlayRaw(0x03, 0x09); // 十四
    }
    else if(num == 15)
    {
        XRVoice_PlayRaw(0x03, 0x0A); // 十五
    }
    else if(num == 16)
    {
        XRVoice_PlayRaw(0x03, 0x0B); // 十六
    }
    else if(num == 17)
    {
        XRVoice_PlayRaw(0x03, 0x0C); // 十七
    }
    else if(num == 18)
    {
        XRVoice_PlayRaw(0x04, 0x00); // 十八
    }
    else if(num == 19)
    {
        XRVoice_PlayRaw(0x04, 0x01); // 十九
    }
    else if(num == 20)
    {
        XRVoice_PlayRaw(0x04, 0x02); // 二十
    }
    else if(num == 21)
    {
        XRVoice_PlayRaw(0x04, 0x03); // 二十一
    }
    else if(num == 22)
    {
        XRVoice_PlayRaw(0x04, 0x04); // 二十二
    }
    else if(num == 23)
    {
        XRVoice_PlayRaw(0x04, 0x05); // 二十三
    }
    else if(num == 24)
    {
        XRVoice_PlayRaw(0x04, 0x06); // 二十四
    }
    else if(num == 25)
    {
        XRVoice_PlayRaw(0x04, 0x07); // 二十五
    }
    else if(num == 26)
    {
        XRVoice_PlayRaw(0x04, 0x08); // 二十六
    }
    else if(num == 27)
    {
        XRVoice_PlayRaw(0x04, 0x09); // 二十七
    }
    else if(num == 28)
    {
        XRVoice_PlayRaw(0x04, 0x10); // 二十八
    }
    else if(num == 29)
    {
        XRVoice_PlayRaw(0x04, 0x11); // 二十九
    }
    else if(num == 30)
    {
        XRVoice_PlayRaw(0x04, 0x12); // 三十
    }
    else if(num == 31)
    {
        XRVoice_PlayRaw(0x04, 0x13); // 三十一
    }
    else if(num == 32)
    {
        XRVoice_PlayRaw(0x04, 0x14); // 三十二
    }
    else if(num == 33)
    {
        XRVoice_PlayRaw(0x04, 0x15); // 三十三
    }
    else if(num == 34)
    {
        XRVoice_PlayRaw(0x04, 0x16); // 三十四
    }
    else if(num == 35)
    {
        XRVoice_PlayRaw(0x04, 0x17); // 三十五
    }
    else if(num == 36)
    {
        XRVoice_PlayRaw(0x04, 0x18); // 三十六
    }
    else if(num == 37)
    {
        XRVoice_PlayRaw(0x04, 0x19); // 三十七
    }
    else if(num == 38)
    {
        XRVoice_PlayRaw(0x04, 0x20); // 三十八
    }
    else if(num == 39)
    {
        XRVoice_PlayRaw(0x04, 0x21); // 三十九
    }
    else if(num == 40)
    {
        XRVoice_PlayRaw(0x04, 0x22); // 四十
    }
    else if(num == 41)
    {
        XRVoice_PlayRaw(0x04, 0x23); // 四十一
    }
    else if(num == 42)
    {
        XRVoice_PlayRaw(0x04, 0x24); // 四十二
    }
    else if(num == 43)
    {
        XRVoice_PlayRaw(0x04, 0x25); // 四十三
    }
    else if(num == 44)
    {
        XRVoice_PlayRaw(0x04, 0x26); // 四十四
    }
    else if(num == 45)
    {
        XRVoice_PlayRaw(0x04, 0x27); // 四十五
    }
    else if(num == 46)
    {
        XRVoice_PlayRaw(0x04, 0x28); // 四十六
    }
    else if(num == 47)
    {
        XRVoice_PlayRaw(0x04, 0x29); // 四十七
    }
    else if(num == 48)
    {
        XRVoice_PlayRaw(0x04, 0x30); // 四十八
    }
    else if(num == 49)
    {
        XRVoice_PlayRaw(0x04, 0x31); // 四十九
    }
    else if(num == 50)
    {
        XRVoice_PlayRaw(0x04, 0x32); // 五十
    }
    else if(num == 51)
    {
        XRVoice_PlayRaw(0x04, 0x33); // 五十一
    }
    else if(num == 52)
    {
        XRVoice_PlayRaw(0x04, 0x34); // 五十二
    }
    else if(num == 53)
    {
        XRVoice_PlayRaw(0x04, 0x35); // 五十三
    }
    else if(num == 54)
    {
        XRVoice_PlayRaw(0x04, 0x36); // 五十四
    }
    else if(num == 55)
    {
        XRVoice_PlayRaw(0x04, 0x37); // 五十五
    }
    else if(num == 56)
    {
        XRVoice_PlayRaw(0x04, 0x38); // 五十六
    }
    else if(num == 57)
    {
        XRVoice_PlayRaw(0x04, 0x39); // 五十七
    }
    else if(num == 58)
    {
        XRVoice_PlayRaw(0x04, 0x40); // 五十八
    }
    else if(num == 59)
    {
        XRVoice_PlayRaw(0x04, 0x41); // 五十九
    }
    else if(num == 60)
    {
        XRVoice_PlayRaw(0x04, 0x42); // 六十
    }
}

// 播放当前时间
void Voice_Play_Current_Time(void)
{
    uint8_t hour, minute, second;
    Get_Current_Time(&hour, &minute, &second);
    
    // 播放小时
    if(hour == 10)
    {
        XRVoice_PlayRaw(0x03, 0x05); // 十
    }
    else if(hour == 11)
    {
        XRVoice_PlayRaw(0x03, 0x06); // 十一
    }
    else if(hour == 12)
    {
        XRVoice_PlayRaw(0x03, 0x07); // 十二
    }
    else if(hour == 13)
    {
        XRVoice_PlayRaw(0x03, 0x08); // 十三
    }
    else if(hour == 14)
    {
        XRVoice_PlayRaw(0x03, 0x09); // 十四
    }
    else if(hour == 15)
    {
        XRVoice_PlayRaw(0x03, 0x0A); // 十五
    }
    else if(hour == 16)
    {
        XRVoice_PlayRaw(0x03, 0x0B); // 十六
    }
    else if(hour == 17)
    {
        XRVoice_PlayRaw(0x03, 0x0C); // 十七
    }
    else if(hour == 18)
    {
        XRVoice_PlayRaw(0x04, 0x00); // 十八
    }
    else if(hour == 19)
    {
        XRVoice_PlayRaw(0x04, 0x01); // 十九
    }
    else if(hour == 20)
    {
        XRVoice_PlayRaw(0x04, 0x02); // 二十
    }
    else if(hour == 21)
    {
        XRVoice_PlayRaw(0x04, 0x03); // 二十一
    }
    else if(hour == 22)
    {
        XRVoice_PlayRaw(0x04, 0x04); // 二十二
    }
    else if(hour == 23)
    {
        XRVoice_PlayRaw(0x04, 0x05); // 二十三
    }
    else if(hour == 0)
    {
        XRVoice_PlayRaw(0x02, 0x08); // 零
    }
    else
    {
        // 1-9直接播放数字
        Voice_Play_Number(hour);
    }
    vTaskDelay(pdMS_TO_TICKS(150));
    
    // 播放"点"
    Voice_Play_Dot();
    vTaskDelay(pdMS_TO_TICKS(150));
    
    // 播放完整的分钟数
    Voice_Play_Number_Full(minute);
	
	vTaskDelay(pdMS_TO_TICKS(230));
	//分
	XRVoice_PlayRaw(0x10, 0x01);
}

// 播放当前时间（带时间段）
void Voice_Play_Current_Time_With_Period(void)
{
    uint8_t hour, minute, second;
    Get_Current_Time(&hour, &minute, &second);
    
    // 切换到屏幕二的时钟界面
    display_mode = 1;
    
    // 根据时间判断时间段
    if(hour < 5)
    {
        XRVoice_PlayRaw(0x08, 0x03); // 现在是凌晨
    }
    else if(hour >= 5 && hour < 9)
    {
        XRVoice_PlayRaw(0x08, 0x04); // 现在是早上
    }
    else if(hour >= 9 && hour < 12)
    {
        // 上午没有单独的语音，使用早上
        XRVoice_PlayRaw(0x08, 0x04); // 现在是早上
    }
    else if(hour >= 12 && hour < 14)
    {
        XRVoice_PlayRaw(0x08, 0x05); // 现在是中午
    }
    else if(hour >= 14 && hour < 18)
    {
        XRVoice_PlayRaw(0x08, 0x06); // 现在是下午
    }
    else if(hour >= 18 && hour < 20)
    {
        XRVoice_PlayRaw(0x08, 0x07); // 现在是傍晚
    }
    else
    {
        XRVoice_PlayRaw(0x08, 0x08); // 现在是晚上
    }
    vTaskDelay(pdMS_TO_TICKS(500));
    
    // 播放当前时间
    Voice_Play_Current_Time();
}

uint8_t ReadDHT11Data(void)
{
    uint8_t retry = 3;
    uint8_t success = 0;
    
    while(retry-- > 0 && !success)
    {
        __disable_irq();
        DHT11_REC_Data();
        __enable_irq();
        
        if(rec_data[0] != 0 || rec_data[2] != 0)
        {
            if(rec_data[0] <= 100 && rec_data[2] <= 60)
            {
                humidity = rec_data[0] + rec_data[1] * 0.1f;
                temperature = rec_data[2] + rec_data[3] * 0.1f;
                success = 1;
            }
        }
        
        if(!success)
        {
            vTaskDelay(pdMS_TO_TICKS(20));
        }
    }
    
    return success;
}

void CheckAndTriggerUpload(void)
{
    static TickType_t last_upload_tick = 0;
    TickType_t now = xTaskGetTickCount();
    
    // 检查上传间隔
    if((now - last_upload_tick) < pdMS_TO_TICKS(UPLOAD_MIN_INTERVAL))
    {
        return;
    }
    
    // 检查温度变化
    if(fabs(temperature - last_upload_temp) >= TEMP_UPLOAD_THRESHOLD)
    {
        g_upload_trigger_temp = 1;
        g_upload_status = UPLOAD_TYPE_TEMP;  // 设置显示状态
        g_upload_display_tick = now;
        last_upload_tick = now;
    }
    // 检查湿度变化
    else if(fabs(humidity - last_upload_humid) >= HUMID_UPLOAD_THRESHOLD)
    {
        g_upload_trigger_humid = 1;
        g_upload_status = UPLOAD_TYPE_HUMID;  // 设置显示状态
        g_upload_display_tick = now;
        last_upload_tick = now;
    }
}

void Fast_UploadData(uint8_t type)
{
    char upload_payload[32];
    
    if(type == 1)
    {
        sprintf(upload_payload, "%.1f", temperature);
        ESP_MQTTPublish((uint8_t*)TOPIC_TEMP, (uint8_t*)upload_payload);
        last_upload_temp = temperature;
        g_upload_status = 1;
    }
    else
    {
        sprintf(upload_payload, "%.1f", humidity);
        ESP_MQTTPublish((uint8_t*)TOPIC_HUMID, (uint8_t*)upload_payload);
        last_upload_humid = humidity;
        g_upload_status = 2;
    }
    
    g_upload_display_tick = xTaskGetTickCount();
}

void Update_Time(void)
{
    if(xTimeMutex != NULL) {
        xSemaphoreTake(xTimeMutex, portMAX_DELAY);
    }
    
    current_second++;
    if(current_second >= 60)
    {
        current_second = 0;
        current_minute++;
        if(current_minute >= 60)
        {
            current_minute = 0;
            current_hour++;
            if(current_hour >= 24)
            {
                current_hour = 0;
            }
        }
    }
    
    if(xTimeMutex != NULL) {
        xSemaphoreGive(xTimeMutex);
    }
}

void Get_Current_Time(uint8_t *hour, uint8_t *minute, uint8_t *second)
{
    if(xTimeMutex != NULL) {
        xSemaphoreTake(xTimeMutex, portMAX_DELAY);
    }
    *hour = current_hour;
    *minute = current_minute;
    *second = current_second;
    
    if(xTimeMutex != NULL) {
        xSemaphoreGive(xTimeMutex);
    }
}

uint8_t Calibrate_Time(uint8_t new_hour, uint8_t new_minute, uint8_t new_second, uint8_t force)
{
    static uint8_t first_sync = 1;
    int32_t diff;
    uint8_t need_update = 0;
    
    if(xTimeMutex != NULL) {
        xSemaphoreTake(xTimeMutex, portMAX_DELAY);
    }
    
    int32_t sys_total_sec = current_hour * 3600 + current_minute * 60 + current_second;
    int32_t bt_total_sec = new_hour * 3600 + new_minute * 60 + new_second;
    diff = bt_total_sec - sys_total_sec;
    
    if(diff > 43200) diff -= 86400;
    else if(diff < -43200) diff += 86400;
    
    if(first_sync || force || abs(diff) > 3)
    {
        current_hour = new_hour;
        current_minute = new_minute;
        current_second = new_second;
        g_last_sync_tick = xTaskGetTickCount();
        g_sync_status = 1;
        first_sync = 0;
        need_update = 1;
    }
    else
    {
        g_last_sync_tick = xTaskGetTickCount();
        need_update = 0;
    }
    
    if(xTimeMutex != NULL) {
        xSemaphoreGive(xTimeMutex);
    }
    
    return need_update;
}

uint8_t Is_Sync_Valid(void)
{
    if(g_sync_status == 0) return 0;
    
    TickType_t now = xTaskGetTickCount();
    if((now - g_last_sync_tick) > pdMS_TO_TICKS(SYNC_VALID_DURATION_MS))
    {
        return 0;
    }
    return 1;
}

uint8_t Check_Alarm_Time(void)
{
    uint8_t hour, minute, second;
    Get_Current_Time(&hour, &minute, &second);
    
    extern volatile HC06_Alarm_t g_bt_alarms[3];
    
    if(hour == g_bt_alarms[0].hour && minute == g_bt_alarms[0].minute && second == 0 && g_bt_alarms[0].enabled) {
        return 1;
    }
    else if(hour == g_bt_alarms[1].hour && minute == g_bt_alarms[1].minute && second == 0 && g_bt_alarms[1].enabled) {
        return 2;
    }
    else if(hour == g_bt_alarms[2].hour && minute == g_bt_alarms[2].minute && second == 0 && g_bt_alarms[2].enabled) {
        return 3;
    }
    
    return 0;
}

void UpdateDisplay(void)
{
    OLED_Clear();
}

void AppTaskCreate(void)
{
    taskENTER_CRITICAL();
    
    xTaskCreate(HX711_Task, "HX711_Task", 512, NULL, 4, &HX711_Task_Handle);
    xTaskCreate(Display_Task, "Display_Task", 512, NULL, 3, &Display_Task_Handle);
    xTaskCreate(DHT11_Task, "DHT11_Task", 512, NULL, 2, &DHT11_Task_Handle);
    xTaskCreate(Upload_Task, "Upload_Task", 256, NULL, 3, &Upload_Task_Handle);
    xTaskCreate(Key_Task, "Key_Task", 256, NULL, 4, &Key_Task_Handle); // 优先级从3升到4，比显示任务高
    xTaskCreate(Time_Task, "Time_Task", 512, NULL, 4, &Time_Task_Handle);
    xTaskCreate(Servo_Task, "Servo_Task", 256, NULL, 3, &Servo_Task_Handle);
	xTaskCreate(Voice_Task, "Voice_Task", 256, NULL, 6, &Voice_Task_Handle);
	
    vTaskDelete(AppTaskCreate_Handle);
    taskEXIT_CRITICAL();
}

void HX711_Task(void* parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(200);
    
    static uint8_t was_heavy = 0;              // 曾经重过（>15g）
    static uint8_t trigger_delay = 0;          // 触发延迟计数
    static uint8_t empty_box_alarm = 0;
    static uint32_t last_play_tick = 0;        // 上次播放时间
    static s32 filtered_weight = 0;            // 一阶低通滤波缓存，整数存储
    static uint8_t sync_counter = 0;            // 兜底同步计数器：200ms一次，强制同步一次
    
    while (1)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        portENTER_CRITICAL();
        Get_Weight();
        s32 raw_weight = Weight_Shiwu;
        portEXIT_CRITICAL();
        
        // 一阶低通滤波：新值占75%权重，兼顾响应速度和平滑度
        filtered_weight = (filtered_weight * 1 + raw_weight * 3) / 4;
        // 重量变化小于1g直接保持旧值，彻底消抖
        if(abs(filtered_weight - weight) >= 1) {
            weight = filtered_weight;
        }
        
        if(tare_done)
        {
            // ========== 兜底同步：2秒检查一次重量，不一致才上传，不浪费流量 ==========
            sync_counter++;
            if(sync_counter >= 10) { // 200ms/次 × 10次 = 2秒
                // 只有当前重量和上次上传的重量差超过1g才触发上传，相同数据不重复传
                if(abs(weight - last_upload_weight) >= 1) {
                    g_upload_trigger_weight = 1;
                }
                sync_counter = 0;
            }
            // ========== 状态机：检测重量变化 ==========
			CheckAndTriggerUpload_Weight(); // 检查重量是否需要上传
            // ========== 新增：检查是否需要停止报警 ==========
            if(g_stop_empty_box_alarm && empty_box_alarm)
            {
                empty_box_alarm = 0;
                g_empty_box_alarm_active = 0;
                last_play_tick = 0;
                LED0_OFF();
                g_stop_empty_box_alarm = 0;  // 清除标志
                was_heavy = 0;
                trigger_delay = 0;
            }
            // 1. 重量>2g：记录"重过"状态，退出空盒状态（降低阈值适配轻药片）
            if(filtered_weight > 2) {
                was_heavy = 1;
                trigger_delay = 0;
                
                // 如果之前在空盒状态，现在退出
                if(empty_box_alarm) {
                    empty_box_alarm = 0;
                    g_empty_box_alarm_active = 0; // 同步复位全局状态
                    last_play_tick = 0;
                    LED0_OFF();
					Servo_Close();
                }
            }
            // 2. 曾经重过且现在<1g：进入空盒状态
            else if(was_heavy && filtered_weight < 1) {
                trigger_delay++;
                
                // 连续3次确认后，进入空盒状态（增加确认次数避免传感器漂移误触发）
                if(trigger_delay >= 3) {
                    empty_box_alarm = 1;   // 进入空盒状态（关键：保持这个标志）
					g_empty_box_alarm_active = 1;  // 设置全局标志
                    trigger_delay = 0;
                    was_heavy = 0;         // 重置，准备下次从重到轻的检测
                    Servo_Open();//开盖
                    // 首次播报
                    if(display_mode == 0) {
                        OLED_ShowString(4, 1, "EMPTY BOX!      ");
                    }
                    // 代码驱动播放空盒报警响应
                    XRVoice_PlayRaw(0x09, 0x00);
                    last_play_tick = xTaskGetTickCount();
                    // 空盒触发时强制上传重量
                    g_upload_trigger_weight = 1;
                    g_upload_status = UPLOAD_TYPE_WEIGHT;
                    g_upload_display_tick = last_play_tick;
                }
            }
            // 3. 重量在1-2g之间：重置延迟计数
            else {
                trigger_delay = 0;
            }
            
            // ========== 空盒状态下的持续行为 ==========
            if(empty_box_alarm)
{
    // 闪烁LED，每200ms翻转 = 2.5Hz
    LED0_Turn();
    
    // 每5秒重复提醒一次
    if((xTaskGetTickCount() - last_play_tick) >= pdMS_TO_TICKS(5*TIME_TASK_DELAY_MS))
    {
        // 代码驱动播放空盒报警响应
        XRVoice_PlayRaw(0x09, 0x00);
        last_play_tick = xTaskGetTickCount();
    }
    
    // 检查是否退出空盒状态（重量恢复> 2g，适配轻药片）
    if(filtered_weight > 2) {
        empty_box_alarm = 0;
        last_play_tick = 0;
        LED0_OFF();
        was_heavy = 1;
        Servo_Close();
    }
}
        }
    }
}

//xiaoR
static void Voice_Command_Callback(uint8_t cmd_type, uint8_t cmd_id)
{
    HandleVoiceCommand(cmd_type, cmd_id);
}

// ==================== 语音任务 ====================
extern SemaphoreHandle_t xVoiceSemaphore;

void Voice_Task(void* parameter)
{
    XRVoice_Init(Voice_Command_Callback);
    XRVoice_SetVolume(g_volume); // 初始化设置音量
    
    vTaskDelay(pdMS_TO_TICKS(100));
    
    // 播放欢迎语
    static uint8_t welcome_played = 0;
    if(!welcome_played) {
        XRVoice_PlayRaw(0x08, 0x01); // 播放"欢迎使用智能药盒"
        // vTaskDelay(pdMS_TO_TICKS(2000)); // 等待语音播放完成 - 已注释掉，不等待语音播放完成，避免阻塞任务
        welcome_played = 1;
    }
    
    while (1)
    {
        // 等待语音指令信号，超时10ms
        if(xSemaphoreTake(xVoiceSemaphore, pdMS_TO_TICKS(10)) == pdTRUE)
        {
            // 有指令到达，立即处理
            XRVoice_Task();
        }
        else
        {
            // 没有指令时也定期调用，用于超时检查
            XRVoice_Task();
        }
    }
}

void DHT11_Task(void* parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(500);
    
    while (1)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        __disable_irq();
        DHT11_REC_Data();
        __enable_irq();
        
        uint8_t success = 0;
        if(rec_data[0] != 0 && rec_data[2] != 0 && 
           rec_data[0] <= 100 && rec_data[2] <= 60)
        {
            humidity = rec_data[0] + rec_data[1] * 0.1f;
            temperature = rec_data[2] + rec_data[3] * 0.1f;
            success = 1;
            dht11_last_status = 1;
            CheckAndTriggerUpload();
        }
        else
        {
            dht11_last_status = 2;
        }
        
        if(display_mode == 0 && !success)
        {
            OLED_ShowChar(2, 16, 'F');
        }
        else if(display_mode == 0 && success)
        {
            OLED_ShowChar(2, 16, ' ');
        }
    }
}

void Display_Task(void* parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(50); // 从100ms改到50ms，加快蓝牙指令响应速度
    
    uint32_t last_temp = 0xFFFFFFFF, last_humid = 0xFFFFFFFF, last_weight = 0xFFFFFFFF;
    uint8_t last_mode = 0xFF;
    uint8_t last_has_bt = 0xFF;
    uint8_t force_refresh = 1;
    uint8_t last_upload_status = 0;
    uint8_t last_dht_status = 0;
    uint8_t last_sync_valid = 0xFF;
    
    uint8_t prev_hour = 0xFF, prev_minute = 0xFF, prev_second = 0xFF;
    
    // 屏幕3编辑模式相关
    uint8_t last_edit_mode = 0xFF;
    uint8_t last_edit_field = 0xFF;
    uint8_t last_volume = 0xFF;
    
    while (1)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        
        HC06_Time_t current_bt_time;
        uint8_t has_new_bt;
        uint8_t dht_status;
        uint8_t sync_valid;
        uint8_t disp_hour, disp_minute, disp_second;
        
        disp_hour = current_hour;
        disp_minute = current_minute;
        disp_second = current_second;
        
        uint8_t current_edit_mode, current_edit_field, current_volume;
        
        taskENTER_CRITICAL();
        current_bt_time.hour = g_bt_time.hour; 
        current_bt_time.minute = g_bt_time.minute;
        current_bt_time.second = g_bt_time.second;
        // 先读数据再取标志，避免中间来数据被清零丢包
        has_new_bt = g_bt_time_updated;
        if(has_new_bt) {
            g_bt_time_updated = 0;
        }
        uint8_t current_mode = display_mode;
        uint8_t key_pressed = g_key1_pressed;
        uint8_t upload_status = g_upload_status;
        dht_status = dht11_last_status;
        sync_valid = Is_Sync_Valid();
        current_edit_mode = g_edit_mode;
        current_edit_field = g_edit_field;
        current_volume = g_volume;
        if(key_pressed) g_key1_pressed = 0;
        taskEXIT_CRITICAL();
        
        if(upload_status != 0) {
            if((xTaskGetTickCount() - g_upload_display_tick) > pdMS_TO_TICKS(400)) {
                upload_status = 0;
                g_upload_status = 0;
                force_refresh = 1;
            }
        }
        
        if(has_new_bt) {
            // 蓝牙主动发过来的时间都是用户设置的有效时间，直接更新
            // 13:10的错误时间已经在hc06.c底层过滤，不会到这里
            Calibrate_Time(current_bt_time.hour, current_bt_time.minute, current_bt_time.second, 1); // force=1强制更新
        }
        
        uint8_t mode_changed = (current_mode != last_mode);
        uint8_t time_changed = (disp_hour != prev_hour || 
                               disp_minute != prev_minute || 
                               disp_second != prev_second);
        uint8_t bt_status_changed = (has_new_bt != last_has_bt);
        uint8_t upload_changed = (upload_status != last_upload_status);
        uint8_t dht_changed = (dht_status != last_dht_status);
        uint8_t sync_changed = (sync_valid != last_sync_valid);
        uint8_t edit_changed = (current_edit_mode != last_edit_mode) || 
                               (current_edit_field != last_edit_field);
        uint8_t volume_changed = (current_volume != last_volume);
        
        if(mode_changed || key_pressed || upload_changed || bt_status_changed || 
           sync_changed || time_changed || edit_changed || volume_changed) {
            force_refresh = 1;
            if(mode_changed) OLED_Clear();
        }
        
        prev_hour = disp_hour;
        prev_minute = disp_minute;
        prev_second = disp_second;
        
        switch(current_mode) {
            case 0:  // 屏幕一：主界面显示温湿度重量
            {
                uint32_t current_temp = (uint32_t)(temperature * 10);
                uint32_t current_humid = (uint32_t)(humidity * 10);
                uint32_t current_weight_val = (uint32_t)weight;
                
                uint8_t data_changed = (current_temp != last_temp) || 
                                       (current_humid != last_humid) ||
                                       (current_weight_val != last_weight);
                
                uint8_t need_update = force_refresh || data_changed || 
                                     upload_changed || dht_changed;
                
                if(need_update)
                {
                    // 温度显示
                    OLED_ShowString(1, 1, "T:");
                    if(temperature >= 0) {
                        OLED_ShowNum(1, 3, (uint32_t)temperature, 2);
                        OLED_ShowString(1, 5, ".");
                        OLED_ShowNum(1, 6, (uint32_t)(temperature * 10) % 10, 1);
                        OLED_ShowChar(1, 7, 'C');
                        OLED_ShowString(1, 8, "  ");
                    } else {
                        OLED_ShowString(1, 3, "-");
                        OLED_ShowNum(1, 4, (uint32_t)(-temperature), 2);
                        OLED_ShowString(1, 6, ".");
                        OLED_ShowNum(1, 7, (uint32_t)(-temperature * 10) % 10, 1);
                        OLED_ShowChar(1, 8, 'C');
                    }
                    
                    // 湿度显示
                    OLED_ShowString(2, 1, "H:");
                    OLED_ShowNum(2, 3, (uint32_t)humidity, 2);
                    OLED_ShowString(2, 5, ".");
                    OLED_ShowNum(2, 6, (uint32_t)(humidity * 10) % 10, 1);
                    OLED_ShowString(2, 7, "%    ");
                    
                    // 重量显示
                    OLED_ShowString(3, 1, "W:");
                    if(weight < 0) {
                        OLED_ShowString(3, 3, "-");
                        OLED_ShowNum(3, 4, (uint32_t)(-weight), 5);
                    } else {
                        OLED_ShowString(3, 3, " ");
                        OLED_ShowNum(3, 4, (uint32_t)weight, 5);
                    }
                    OLED_ShowString(3, 9, "g    ");
                    
                    // 上传状态显示
                    if(upload_status == UPLOAD_TYPE_TEMP) {
						OLED_ShowString(4, 1, "Temp Uploaded   ");
					} else if(upload_status == UPLOAD_TYPE_HUMID) {
						OLED_ShowString(4, 1, "Humid Uploaded  ");
					} else if(upload_status == UPLOAD_TYPE_WEIGHT) {
						OLED_ShowString(4, 1, "Weight Uploaded ");
					} else if(upload_status == UPLOAD_TYPE_STATUS) {
						OLED_ShowString(4, 1, "Status ");
						// 显示盒子状态作为主要状态
						OLED_ShowNum(4, 8, g_box_state, 1);
						OLED_ShowString(4, 9, " Uploaded");
					} else {
						OLED_ShowString(4, 1, "                ");  // 清空
					}
                    
                    // ========== 数据变化时立即触发上传检查 ==========
                    if(data_changed) {
                        CheckAndTriggerUpload();
                    }
                    
                    last_temp = current_temp;
                    last_humid = current_humid;
                    last_weight = current_weight_val;
                    force_refresh = 0;
                }
                break;
            }
            
            case 1:  // 屏幕二：时间显示
            {
                static uint32_t last_display_tick = 0;
                static uint8_t last_h1 = 0xFF, last_h2 = 0xFF;
                static uint8_t last_m1 = 0xFF, last_m2 = 0xFF;
                static uint8_t last_s1 = 0xFF, last_s2 = 0xFF;
                static uint8_t colon_drawn = 0;
                uint32_t now = xTaskGetTickCount();
                
                uint8_t h1 = disp_hour / 10, h2 = disp_hour % 10;
                uint8_t m1 = disp_minute / 10, m2 = disp_minute % 10;
                uint8_t s1 = disp_second / 10, s2 = disp_second % 10;
                
                uint8_t h1_changed = (h1 != last_h1);
                uint8_t h2_changed = (h2 != last_h2);
                uint8_t m1_changed = (m1 != last_m1);
                uint8_t m2_changed = (m2 != last_m2);
                uint8_t s1_changed = (s1 != last_s1);
                uint8_t s2_changed = (s2 != last_s2);
                uint8_t any_num_changed = h1_changed || h2_changed || m1_changed || 
                                          m2_changed || s1_changed || s2_changed;
                
                if(any_num_changed || force_refresh || 
                   (now - last_display_tick) >= pdMS_TO_TICKS(1000))
                {
                    last_display_tick = now;
                    
                    static uint8_t last_sync = 0xFF;
                    if(sync_valid != last_sync || force_refresh) {
                        last_sync = sync_valid;
                        if(sync_valid) {
                            OLED_ShowString(1, 1, "[BT Sync]       ");
                        } else {
                            OLED_ShowString(1, 1, "[No Sync]       ");
                        }
                    }
                    
                    if(h1_changed || force_refresh) {
                        OLED_Show16x32Char(2, 1, '0' + h1);
                        last_h1 = h1;
                    }
                    
                    if(h2_changed || force_refresh) {
                        OLED_Show16x32Char(2, 3, '0' + h2);
                        last_h2 = h2;
                    }
                    
                    if(!colon_drawn || force_refresh) {
                        OLED_Show16x32Char(2, 5, ':');
                        colon_drawn = 1;
                    }
                    
                    if(m1_changed || force_refresh) {
                        OLED_Show16x32Char(2, 7, '0' + m1);
                        last_m1 = m1;
                    }
             
                    if(m2_changed || force_refresh) {
                        OLED_Show16x32Char(2, 9, '0' + m2);
                        last_m2 = m2;
                    }
                    
                    if(!colon_drawn || force_refresh) {
                        OLED_Show16x32Char(2, 11, ':');
                    }
                    
                    if(s1_changed || force_refresh) {
                        OLED_Show16x32Char(2, 13, '0' + s1);
                        last_s1 = s1;
                    }
                    
                    if(s2_changed || force_refresh) {
                        OLED_Show16x32Char(2, 15, '0' + s2);
                        last_s2 = s2;
                    }
                    
                    OLED_ShowString(4, 1, "                ");
                    
                    force_refresh = 0;
                }
                break;
            }

            case 2:  // 屏幕三：闹钟设置
            {
                if(g_bt_alarm_updated) {
                    force_refresh = 1;
                }
                
                if(force_refresh || edit_changed || volume_changed)
                {
                    // 第1行：标题
                    if(current_edit_mode == EDIT_MODE_NONE) {
                        OLED_ShowString(1, 1, "  Alarm Clocks  ");
                    } else {
                        OLED_ShowString(1, 1, "    EDITING     ");
                    }
                    
                    // 第2行：闹钟1
                    OLED_ShowString(2, 1, "1:");
                    if(current_edit_mode == EDIT_MODE_ALARM1) {
                        if(current_edit_field == 0) {
                            OLED_ShowChar(2, 3, '>');
                            OLED_ShowNum(2, 4, g_bt_alarms[0].hour, 2);
                            OLED_ShowChar(2, 6, '<');
                            OLED_ShowChar(2, 7, ':');
                            OLED_ShowNum(2, 8, g_bt_alarms[0].minute, 2);
                            OLED_ShowString(2, 10, "  ");
                        } else {
                            OLED_ShowNum(2, 3, g_bt_alarms[0].hour, 2);
                            OLED_ShowChar(2, 5, ':');
                            OLED_ShowChar(2, 6, '>');
                            OLED_ShowNum(2, 7, g_bt_alarms[0].minute, 2);
                            OLED_ShowChar(2, 9, '<');
                            OLED_ShowString(2, 10, "  ");
                        }
                    } else {
                        OLED_ShowNum(2, 3, g_bt_alarms[0].hour, 2);
                        OLED_ShowChar(2, 5, ':');
                        OLED_ShowNum(2, 6, g_bt_alarms[0].minute, 2);
                        OLED_ShowString(2, 8, "        ");
                    }
                    
                    // 第3行：闹钟2
                    OLED_ShowString(3, 1, "2:");
                    if(current_edit_mode == EDIT_MODE_ALARM2) {
                        if(current_edit_field == 0) {
                            OLED_ShowChar(3, 3, '>');
                            OLED_ShowNum(3, 4, g_bt_alarms[1].hour, 2);
                            OLED_ShowChar(3, 6, '<');
                            OLED_ShowChar(3, 7, ':');
                            OLED_ShowNum(3, 8, g_bt_alarms[1].minute, 2);
                            OLED_ShowString(3, 10, "  ");
                        } else {
                            OLED_ShowNum(3, 3, g_bt_alarms[1].hour, 2);
                            OLED_ShowChar(3, 5, ':');
                            OLED_ShowChar(3, 6, '>');
                            OLED_ShowNum(3, 7, g_bt_alarms[1].minute, 2);
                            OLED_ShowChar(3, 9, '<');
                            OLED_ShowString(3, 10, "  ");
                        }
                    } else {
                        OLED_ShowNum(3, 3, g_bt_alarms[1].hour, 2);
                        OLED_ShowChar(3, 5, ':');
                        OLED_ShowNum(3, 6, g_bt_alarms[1].minute, 2);
                        OLED_ShowString(3, 8, "        ");
                    }
                    
                    // 第4行：闹钟3 + 音量
                    OLED_ShowString(4, 1, "3:");
                    
                    if (current_edit_mode == EDIT_MODE_ALARM3) {
                        if (current_edit_field == 0) {
                            OLED_ShowChar(4, 3, '>');
                            OLED_ShowNum(4, 4, g_bt_alarms[2].hour, 2);
                            OLED_ShowChar(4, 6, '<');
                            OLED_ShowChar(4, 7, ':');
                            OLED_ShowNum(4, 8, g_bt_alarms[2].minute, 2);
                            OLED_ShowString(4, 10, " V:");
                            OLED_ShowNum(4, 13, current_volume, 1); // 音量最大是6，直接显示1位即可
                        } else {
                            OLED_ShowNum(4, 3, g_bt_alarms[2].hour, 2);
                            OLED_ShowChar(4, 5, ':');
                            OLED_ShowChar(4, 6, '>');
                            OLED_ShowNum(4, 7, g_bt_alarms[2].minute, 2);
                            OLED_ShowChar(4, 9, '<');
                            OLED_ShowString(4, 10, " V:");
                            OLED_ShowNum(4, 13, current_volume, 1); // 音量最大是6，直接显示1位即可
                        }
                    } else if (current_edit_mode == EDIT_MODE_VOLUME) {
                        OLED_ShowNum(4, 3, g_bt_alarms[2].hour, 2);
                        OLED_ShowChar(4, 5, ':');
                        OLED_ShowNum(4, 6, g_bt_alarms[2].minute, 2);
                        OLED_ShowString(4, 8, " V>");
                        OLED_ShowChar(4, 11, ' ');
                        OLED_ShowNum(4, 12, current_volume, 1); // 音量最大是6，直接显示1位即可
                        OLED_ShowChar(4, 13, '<');
                        OLED_ShowChar(4, 14, ' ');
                    } else {
                        OLED_ShowNum(4, 3, g_bt_alarms[2].hour, 2);
                        OLED_ShowChar(4, 5, ':');
                        OLED_ShowNum(4, 6, g_bt_alarms[2].minute, 2);
                        OLED_ShowString(4, 8, " V:");
                        OLED_ShowChar(4, 12, ' ');
                        OLED_ShowNum(4, 13, current_volume, 1); // 音量最大是6，直接显示1位即可
                        OLED_ShowChar(4, 14, ' ');
                    }
                    
                    force_refresh = 0;
                    last_edit_mode = current_edit_mode;
                    last_edit_field = current_edit_field;
                    last_volume = current_volume;
                }
                break;
            }
        }
        
        last_mode = current_mode;
        last_upload_status = upload_status;
        last_dht_status = dht_status;
        last_sync_valid = sync_valid;
        last_has_bt = has_new_bt;
    }
}

void Upload_Task(void* parameter)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(50));
        ProcessUploadQueue();// 处理上传队列
    }
}

void Time_Task(void* parameter)
{
    uint8_t alarm_triggered = 0;
    uint8_t last_alarm_index = 0;
    static uint32_t run_count = 0;
    
    // 初始化光敏传感器
    LightSenor_Init();
    g_last_light_state = LightSenor_Get();
    
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(TIME_TASK_DELAY_MS));  // 约1秒
        
        run_count++;
        Update_Time();
        
        taskENTER_CRITICAL();
        g_time_task_run_count = run_count;
        taskEXIT_CRITICAL();
        
        // 更新盒子状态
        if(g_servo_state) {
            g_box_state = BOX_OPEN;
        } else {
            g_box_state = BOX_CLOSED;
        }
        
        // 更新闹钟状态
        if(g_alarm_ringing) {
            g_alarm_state = ALARM_RINGING;
        } else {
            g_alarm_state = ALARM_SILENT;
        }
        
        // 更新药盒内容状态（这里需要根据实际的空盒检测逻辑来设置）
        // 假设g_empty_box_alarm_active表示药盒为空
        if(g_empty_box_alarm_active) {
            g_box_content_state = BOX_EMPTY;
        } else {
            g_box_content_state = BOX_NOT_EMPTY;
        }
        
        // 触发状态上传
        CheckAndTriggerUpload_Status();
        
        // 1. 检测光敏变化
        uint8_t current_light = LightSenor_Get();
        if(current_light != g_last_light_state)
        {
            g_light_changed = 1;
            g_last_light_state = current_light;
        }
        
        // 2. 检测闹钟时间
        uint8_t alarm_index = Check_Alarm_Time();
        
        // 3. 闹钟触发处理
        if(alarm_index != 0)
        {
            // 新的闹钟触发
            if(!alarm_triggered || alarm_index != last_alarm_index)
            {
                // 设置闹钟状态
                g_alarm_ringing = 1;
                g_alarm_servo_state = 1; // 进入等待第一次遮挡开盖状态
                g_alarm_start_tick = xTaskGetTickCount();
                g_last_alarm_play_tick = xTaskGetTickCount();
                g_light_changed = 0;
                
                // 代码驱动播放对应的闹钟语音
                // 闹钟1: 早上好，请记得吃药
                // 闹钟2: 中午好，请记得吃药
                // 闹钟3: 晚上好，请记得吃药
                XRVoice_PlayRaw(0x09, alarm_index);  // alarm_index: 1->1, 2->2, 3->3
                
                if(display_mode == 0) {
                    OLED_ShowString(4, 1, "TAKE MEDICINE!  ");
                }
                
                alarm_triggered = 1;
                last_alarm_index = alarm_index;
            }
        }
        
        // 4. 处理闹钟持续状态
        if(g_alarm_ringing)
        {
            // 闪烁LED，每200ms翻转一次 = 2.5Hz
            LED0_Turn();
            
            // 检测到光敏变化（遮挡事件）
            if(g_light_changed)
            {
                g_light_changed = 0;
                // 第一次遮挡：打开盖子
                if(g_alarm_servo_state == 1)
                {
                    Servo_Open();
                    g_alarm_servo_state = 2; // 进入等待第二次遮挡关盖状态
                    if(display_mode == 0) {
                        OLED_ShowString(4, 1, "Lid opened!      ");
                    }
                }
                // 第二次遮挡：关闭盖子+停止闹钟
                else if(g_alarm_servo_state == 2)
                {
                    // 停止闹钟
                    g_alarm_ringing = 0;
                    g_alarm_servo_state = 0;
                    alarm_triggered = 0;
                    
                    // 关闭舵机
                    Servo_Close();
                    
                    // 关闭LED
                    LED0_OFF();
                    
                    if(display_mode == 0) {
                        OLED_ShowString(4, 1, "Medicine taken! ");
                    }
                }
            }
            // 重复提醒（每5秒）
            else if((xTaskGetTickCount() - g_last_alarm_play_tick) >= pdMS_TO_TICKS(TIME_TASK_DELAY_MS*5))
            {
                // 代码驱动重复播放相同的闹钟语音
                XRVoice_PlayRaw(0x09, last_alarm_index);
                g_last_alarm_play_tick = xTaskGetTickCount();
            }
        }
        
        // 5. 重置状态
        if(alarm_index == 0 && !g_alarm_ringing)
        {
            alarm_triggered = 0;
            last_alarm_index = 0;
        }
    }
}

void Key_Task(void* parameter)
{
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequency = pdMS_TO_TICKS(10); // 扫描周期从20ms缩短到10ms，按键响应更快
    
    while (1)
    {
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
        Key_Scan();
        
        // 读取当前显示模式
        uint8_t current_mode;
        taskENTER_CRITICAL();
        current_mode = display_mode;
        taskEXIT_CRITICAL();
        
		// ========== 任意按键停止空盒报警 ==========
        if(g_empty_box_alarm_active)  // 如果空盒报警正在响
        {
            if(Key_GetPress(KEY1) || Key_GetPress(KEY2) || 
               Key_GetPress(KEY3) || Key_GetPress(KEY4))
            {
                g_stop_empty_box_alarm = 1;  // 设置停止标志
                        Servo_Close();
                // 移除阻塞延迟，按键扫描本身已带消抖
            }
        }
        // PB8 - 切换页面（所有屏幕有效）
        if(Key_GetPress(KEY1))
        {
            // 如果在屏幕3的编辑模式，先退出编辑模式
            taskENTER_CRITICAL();
            uint8_t edit_was_active = (g_edit_mode != EDIT_MODE_NONE);
            if(edit_was_active) {
                g_edit_mode = EDIT_MODE_NONE;
            }
            
            display_mode++;
            if(display_mode > 2) {
                display_mode = 0;
            }
            g_key1_pressed = 1;
            uint8_t new_mode = display_mode;
            taskEXIT_CRITICAL();
            
            // 如果是从编辑模式退出，先播报修改完成
            if(edit_was_active) {
                XRVoice_PlayRaw(0x10, 0x05); // 播报修改完成
                // 移除等待播报的400ms阻塞，语音播放是异步的不影响按键响应
            }
            
            // 播报当前界面
            if(new_mode == 0) {
                XRVoice_PlayRaw(0x10, 0x02); // 数据界面（界面一）
            } else if(new_mode == 1) {
                XRVoice_PlayRaw(0x10, 0x03); // 时间界面（界面二）
            } else if(new_mode == 2) {
                XRVoice_PlayRaw(0x10, 0x04); // 闹钟界面（界面三）
            }
            
            OLED_Clear();
			
        }
        
        // PB9 - 屏幕3：进入/退出编辑，切换时分
        if(Key_GetPress(KEY2))
        {
            // 只在屏幕3有效
            if(current_mode == 2)
            {
                taskENTER_CRITICAL();
                uint8_t current_edit = g_edit_mode;
                uint8_t current_field = g_edit_field;
                
                if(current_edit == EDIT_MODE_NONE) {
                    // 进入闹钟1编辑模式
                    g_edit_mode = EDIT_MODE_ALARM1;
                    g_edit_field = 0;  // 默认编辑小时
                }
                else if(current_edit == EDIT_MODE_ALARM1) {
                    if(current_field == 0) {
                        // 小时编辑完成，切换到分钟
                        g_edit_field = 1;
                    } else {
                        // 分钟编辑完成，切换到闹钟2
                        g_edit_mode = EDIT_MODE_ALARM2;
                        g_edit_field = 0;
                    }
                }
                else if(current_edit == EDIT_MODE_ALARM2) {
                    if(current_field == 0) {
                        g_edit_field = 1;
                    } else {
                        g_edit_mode = EDIT_MODE_ALARM3;
                        g_edit_field = 0;
                    }
                }
                else if(current_edit == EDIT_MODE_ALARM3) {
                    if(current_field == 0) {
                        g_edit_field = 1;
                    } else {
                        // 进入音量编辑
                        g_edit_mode = EDIT_MODE_VOLUME;
                    }
                }
                else if(current_edit == EDIT_MODE_VOLUME) {
                    // 音量编辑完成，退出编辑模式
                    g_edit_mode = EDIT_MODE_NONE;
                    
                    // 播报修改完成
                    XRVoice_PlayRaw(0x10, 0x05);
                }
                taskEXIT_CRITICAL();
            }
        }
        
        // PB6 - 屏幕3：减小
        if(Key_GetPress(KEY3))
        {
            if(current_mode == 2)
            {
                taskENTER_CRITICAL();
                uint8_t current_edit = g_edit_mode;
                uint8_t current_field = g_edit_field;
                
                if(current_edit == EDIT_MODE_ALARM1) {
                    if(current_field == 0) {
                        // 减小小时
                        if(g_bt_alarms[0].hour > 0) {
                            g_bt_alarms[0].hour--;
                        } else {
                            g_bt_alarms[0].hour = 23;  // 循环到23
                        }
                    } else {
                        // 减小分钟
                        if(g_bt_alarms[0].minute > 0) {
                            g_bt_alarms[0].minute--;
                        } else {
                            g_bt_alarms[0].minute = 59;  // 循环到59
                        }
                    }
                    g_bt_alarm_updated = 1;  // 标记闹钟已更新
                }
                else if(current_edit == EDIT_MODE_ALARM2) {
                    if(current_field == 0) {
                        if(g_bt_alarms[1].hour > 0) {
                            g_bt_alarms[1].hour--;
                        } else {
                            g_bt_alarms[1].hour = 23;
                        }
                    } else {
                        if(g_bt_alarms[1].minute > 0) {
                            g_bt_alarms[1].minute--;
                        } else {
                            g_bt_alarms[1].minute = 59;
                        }
                    }
                    g_bt_alarm_updated = 1;
                }
                else if(current_edit == EDIT_MODE_ALARM3) {
                    if(current_field == 0) {
                        if(g_bt_alarms[2].hour > 0) {
                            g_bt_alarms[2].hour--;
                        } else {
                            g_bt_alarms[2].hour = 23;
                        }
                    } else {
                        if(g_bt_alarms[2].minute > 0) {
                            g_bt_alarms[2].minute--;
                        } else {
                            g_bt_alarms[2].minute = 59;
                        }
                    }
                    g_bt_alarm_updated = 1;
                }
                else if(current_edit == EDIT_MODE_VOLUME) {
					// 减小音量
					if(g_volume > 1) {
						g_volume--;
						XRVoice_SetVolume(g_volume); // 同步到语音模块
						XRVoice_VolumeDown();
					}
				}
                taskEXIT_CRITICAL();
            }
        }
        
        // PB7 - 屏幕3：增大
        if(Key_GetPress(KEY4))
        {
            if(current_mode == 2)
            {
                taskENTER_CRITICAL();
                uint8_t current_edit = g_edit_mode;
                uint8_t current_field = g_edit_field;
                
                if(current_edit == EDIT_MODE_ALARM1) {
                    if(current_field == 0) {
                        // 增大小時
                        if(g_bt_alarms[0].hour < 23) {
                            g_bt_alarms[0].hour++;
                        } else {
                            g_bt_alarms[0].hour = 0;  // 循环到0
                        }
                    } else {
                        // 增大分钟
                        if(g_bt_alarms[0].minute < 59) {
                            g_bt_alarms[0].minute++;
                        } else {
                            g_bt_alarms[0].minute = 0;  // 循环到0
                        }
                    }
                    g_bt_alarm_updated = 1;
                }
                else if(current_edit == EDIT_MODE_ALARM2) {
                    if(current_field == 0) {
                        if(g_bt_alarms[1].hour < 23) {
                            g_bt_alarms[1].hour++;
                        } else {
                            g_bt_alarms[1].hour = 0;
                        }
                    } else {
                        if(g_bt_alarms[1].minute < 59) {
                            g_bt_alarms[1].minute++;
                        } else {
                            g_bt_alarms[1].minute = 0;
                        }
                    }
                    g_bt_alarm_updated = 1;
                }
                else if(current_edit == EDIT_MODE_ALARM3) {
                    if(current_field == 0) {
                        if(g_bt_alarms[2].hour < 23) {
                            g_bt_alarms[2].hour++;
                        } else {
                            g_bt_alarms[2].hour = 0;
                        }
                    } else {
                        if(g_bt_alarms[2].minute < 59) {
                            g_bt_alarms[2].minute++;
                        } else {
                            g_bt_alarms[2].minute = 0;
                        }
                    }
                    g_bt_alarm_updated = 1;
                }
                else if(current_edit == EDIT_MODE_VOLUME) {
					// 增大音量
					if(g_volume < 6) {
						g_volume++;
						XRVoice_SetVolume(g_volume); // 同步到语音模块
						XRVoice_VolumeUp();
					}
				}
                taskEXIT_CRITICAL();
            }
        }
    }
}
// ==================== 舵机控制函数 ====================

void Servo_SendCommand(Servo_Command_t cmd)
{
    taskENTER_CRITICAL();
    g_servo_cmd = (uint8_t)cmd;
    taskEXIT_CRITICAL();
}

void Servo_Open(void)
{
    Servo_SendCommand(SERVO_CMD_OPEN);
}

void Servo_Close(void)
{
    Servo_SendCommand(SERVO_CMD_CLOSE);
}

// ==================== 舵机任务 ====================
void Servo_Task(void* parameter)
{
    PWM_Init();
    
    // 直接设置初始位置（关闭）
    PWM_SetCompare2(500 + (SERVO_ANGLE_CLOSE * 2000 / 180));
    g_servo_state = 0;
    
    vTaskDelay(pdMS_TO_TICKS(1000));  // 给舵机1秒到位时间

    TickType_t xLastWakeTime = xTaskGetTickCount();
    uint8_t current_cmd = SERVO_CMD_NONE;
    uint8_t last_cmd = SERVO_CMD_NONE;

    while(1)
    {
        vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(100));
        
        taskENTER_CRITICAL();
        current_cmd = g_servo_cmd;
        g_servo_cmd = SERVO_CMD_NONE;
        taskEXIT_CRITICAL();

        if(current_cmd != SERVO_CMD_NONE && current_cmd != last_cmd)
        {
            uint16_t pulse;
            switch(current_cmd)
            {
                case SERVO_CMD_OPEN:
                    pulse = 500 + (SERVO_ANGLE_OPEN * 2000 / 180);
                    PWM_SetCompare2(pulse);
                    g_servo_state = 1;
                    break;
                case SERVO_CMD_CLOSE:
                    pulse = 500 + (SERVO_ANGLE_CLOSE * 2000 / 180);
                    PWM_SetCompare2(pulse);
                    g_servo_state = 0;
                    break;
                default:
                    break;
            }
            last_cmd = current_cmd;
        }
    }
}
