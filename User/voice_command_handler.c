#include "voice_command_handler.h"
#include "xrvoice.h"
#include "OLED.h"
#include "app_tasks.h"
#include "Servo.h"
#include "LED.h"

// 外部函数声明
extern void Voice_Play_Current_Time(void);
extern void Voice_Play_Current_Time_With_Period(void);

// 从app_tasks.c引入的全局变量
extern volatile uint8_t g_alarm_ringing;
extern volatile uint8_t g_light_changed;
extern volatile uint8_t g_empty_box_alarm_active;
extern volatile uint8_t g_stop_empty_box_alarm;
extern volatile uint8_t g_volume;
extern volatile uint8_t display_mode;

// 全局变量
volatile uint8_t g_voice_alarm_edit = 0;          // 0=未在修改, 1=等待输入闹钟序号, 2=等待输入小时, 3=等待输入分钟
volatile uint8_t g_voice_alarm_index = 0;         // 选中的闹钟序号 (1-3)
volatile uint8_t g_voice_alarm_hour = 0;          // 输入的小时
volatile uint8_t g_voice_alarm_minute = 0;        // 输入的分钟

// 刷新闹钟界面OLED显示
void RefreshAlarmDisplay(void)
{
    // 刷新OLED显示
    OLED_Clear();
    // 显示闹钟界面
    OLED_ShowString(1, 1, "  Alarm Clocks  ");
    OLED_ShowString(2, 1, "1:");
    OLED_ShowNum(2, 3, g_bt_alarms[0].hour, 2);
    OLED_ShowChar(2, 5, ':');
    OLED_ShowNum(2, 6, g_bt_alarms[0].minute, 2);
    OLED_ShowString(3, 1, "2:");
    OLED_ShowNum(3, 3, g_bt_alarms[1].hour, 2);
    OLED_ShowChar(3, 5, ':');
    OLED_ShowNum(3, 6, g_bt_alarms[1].minute, 2);
    OLED_ShowString(4, 1, "3:");
    OLED_ShowNum(4, 3, g_bt_alarms[2].hour, 2);
    OLED_ShowChar(4, 5, ':');
    OLED_ShowNum(4, 6, g_bt_alarms[2].minute, 2);
}

// 更新闹钟时间并刷新OLED显示
void UpdateAlarmTime(uint8_t index, uint8_t hour, uint8_t minute)
{
    // 验证闹钟序号、小时(0-23)和分钟(0-59)的有效性
    if(index >= 1 && index <= 3 && hour <= 23 && minute <= 59)
    {
        // 更新闹钟时间
        uint8_t alarm_index = index - 1; // 转换为0-2的索引
        g_bt_alarms[alarm_index].hour = hour;
        g_bt_alarms[alarm_index].minute = minute;
        
        // 刷新OLED显示
        RefreshAlarmDisplay();
        
        // 播报修改完成
        XRVoice_PlayRaw(0x10, 0x05);
    }
}

// 处理语音命令
void HandleVoiceCommand(uint8_t cmd_type, uint8_t cmd_id)
{
    // 处理Type=01命令
    if(cmd_type == 0x01)
    {
        switch(cmd_id)
        {
            case 0x00: // 欢迎语
                break;
            case 0x01: // 休息语
                break;
            case 0x02: // 唤醒词
                break;
            case 0x03: // 增大音量
                if(g_volume < 6) {
                    g_volume++;
                    XRVoice_SetVolume(g_volume); // 同步到语音模块
                }
                break;
            case 0x04: // 减小音量
                if(g_volume > 1) {
                    g_volume--;
                    XRVoice_SetVolume(g_volume); // 同步到语音模块
                }
                break;
            case 0x05: // 最大音量
                g_volume = 6;
                XRVoice_SetVolume(g_volume); // 同步到语音模块
                break;
            case 0x06: // 中等音量
                g_volume = 3;
                XRVoice_SetVolume(g_volume); // 同步到语音模块
                break;
            case 0x07: // 最小音量
                g_volume = 1;
                XRVoice_SetVolume(g_volume); // 同步到语音模块
                break;
            case 0x08: // 开启播报
                break;
            case 0x09: // 关闭播报
                break;
        }
        return;
    }
    
    // 处理Type=08命令（现在是什么时候）
    if(cmd_type == 0x08)
    {
        switch(cmd_id)
        {
            case 0x00: // 现在是什么时候
                Voice_Play_Current_Time_With_Period();
                break;
        }
        return;
    }
    
    // 处理Type=10命令（点）
    if(cmd_type == 0x10)
    {
        switch(cmd_id)
        {
            case 0x00: // 点
                // 这里可以添加点的处理逻辑
                break;
        }
        return;
    }
    
    // 处理Type=02命令
    if(cmd_type == 0x02)
    {
        switch(cmd_id)
        {
            case 0x00: // 联系监护人
                break;
            case 0x01: // 停下
                break;
            case 0x02: // 打开盖子
                Servo_Open();
                break;
            case 0x03: // 关闭盖子
                Servo_Close();
                
                // 停止闹钟状态
                if(g_alarm_ringing)
                {
                    g_alarm_ringing = 0;
                    g_light_changed = 0;
                    LED0_OFF();
                    
                    if(display_mode == 0) {
                        OLED_ShowString(4, 1, "Medicine taken! ");
                    }
                }
                
                // 停止空盒报警
                if(g_empty_box_alarm_active)
                {
                    g_stop_empty_box_alarm = 1;
                    LED0_OFF();
                }
                break;
            case 0x04: // 修改闹钟
                g_voice_alarm_edit = 1;
                display_mode = 2; // 切换到闹钟设置界面
                // 不主动播放语音，等待用户输入
                break;
            case 0x05: // 闹钟一
                if(g_voice_alarm_edit == 1)
                {
                    g_voice_alarm_index = 1;
                    g_voice_alarm_edit = 2; // 进入等待小时输入状态
                    XRVoice_PlayRaw(0x02, 0x05); // 播报"请时钟分钟分开说数字"
                }
                break;
            case 0x06: // 闹钟二
                if(g_voice_alarm_edit == 1)
                {
                    g_voice_alarm_index = 2;
                    g_voice_alarm_edit = 2; // 进入等待小时输入状态
                    XRVoice_PlayRaw(0x02, 0x06); // 播报"请时钟分钟分开说数字"
                }
                break;
            case 0x07: // 闹钟三
                if(g_voice_alarm_edit == 1)
                {
                    g_voice_alarm_index = 3;
                    g_voice_alarm_edit = 2; // 进入等待小时输入状态
                    XRVoice_PlayRaw(0x02, 0x07); // 播报"请时钟分钟分开说数字"
                }
                break;
            case 0x08: // 零
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 0;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x02, 0x08); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 0;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x02, 0x08); // 播报"录入成功"
                }
                break;
            case 0x09: // 一
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 1;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x02, 0x09); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 1;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x02, 0x09); // 播报"录入成功"
                }
                break;
            case 0x0A: // 二
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 2;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x02, 0x0A); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 2;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x02, 0x0A); // 播报"录入成功"
                }
                break;
            case 0x0B: // 三
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 3;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x02, 0x0B); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 3;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x02, 0x0B); // 播报"录入成功"
                }
                break;
            case 0x0C: // 四
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 4;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x02, 0x0C); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 4;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x02, 0x0C); // 播报"录入成功"
                }
                break;
        }
        return;
    }
    
    // 处理Type=03命令
    if(cmd_type == 0x03)
    {
        switch(cmd_id)
        {
            case 0x00: // 五
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 5;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x00); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 5;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x00); // 播报"录入成功"
                }
                break;
            case 0x01: // 六
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 6;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x01); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 6;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x01); // 播报"录入成功"
                }
                break;
            case 0x02: // 七
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 7;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x02); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 7;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x02); // 播报"录入成功"
                }
                break;
            case 0x03: // 八
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 8;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x03); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 8;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x03); // 播报"录入成功"
                }
                break;
            case 0x04: // 九
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 9;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x04); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 9;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x04); // 播报"录入成功"
                }
                break;
            case 0x05: // 十
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 10;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x05); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 10;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x05); // 播报"录入成功"
                }
                break;
            case 0x06: // 十一
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 11;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x06); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 11;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x06); // 播报"录入成功"
                }
                break;
            case 0x07: // 十二
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 12;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x07); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 12;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x07); // 播报"录入成功"
                }
                break;
            case 0x08: // 十三
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 13;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x08); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 13;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x08); // 播报"录入成功"
                }
                break;
            case 0x09: // 十四
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 14;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x09); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 14;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x09); // 播报"录入成功"
                }
                break;
            case 0x0A: // 十五
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 15;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x0A); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 15;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x0A); // 播报"录入成功"
                }
                break;
            case 0x0B: // 十六
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 16;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x0B); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 16;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x0B); // 播报"录入成功"
                }
                break;
            case 0x0C: // 十七
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 17;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x03, 0x0C); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 17;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x03, 0x0C); // 播报"录入成功"
                }
                break;
        }
        return;
    }
    
    // 处理Type=04命令
    if(cmd_type == 0x04)
    {
        // 小时输入验证：0-23
        if(g_voice_alarm_edit == 2 && cmd_id >= 0x06) {
            // 小时输入错误，超出范围
            XRVoice_PlayRaw(0x05, 0x00); // 播报"错误，请重新输入"
            return;
        }
        
        switch(cmd_id)
        {
            case 0x00: // 十八
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 18;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x04, 0x00); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 18;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x00); // 播报"录入成功"
                }
                break;
            case 0x01: // 十九
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 19;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x04, 0x01); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 19;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x01); // 播报"录入成功"
                }
                break;
            case 0x02: // 二十
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 20;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x04, 0x02); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 20;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x02); // 播报"录入成功"
                }
                break;
            case 0x03: // 二十一
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 21;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x04, 0x03); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 21;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x03); // 播报"录入成功"
                }
                break;
            case 0x04: // 二十二
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 22;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x04, 0x04); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 22;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x04); // 播报"录入成功"
                }
                break;
            case 0x05: // 二十三
                if(g_voice_alarm_edit == 2)
                {
                    g_voice_alarm_hour = 23;
                    // 立即更新闹钟时间（分钟保持不变）
                    uint8_t alarm_index = g_voice_alarm_index - 1;
                    if(alarm_index < 3) {
                        g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;
                        // 刷新OLED显示
                        RefreshAlarmDisplay();
                    }
                    g_voice_alarm_edit = 3; // 进入等待分钟输入状态
                    XRVoice_PlayRaw(0x04, 0x05); // 播报"录入成功"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 23;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x05); // 播报"录入成功"
                }
                break;
            case 0x06: // 二十四
                if(g_voice_alarm_edit == 2)
                {
                    // 小时输入错误，超出范围
                    XRVoice_PlayRaw(0x05, 0x00); // 播报"错误，请重新输入"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 24;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x06); // 播报"录入成功"
                }
                break;
            // 其他分钟数字处理...
            case 0x07: // 二十五
                if(g_voice_alarm_edit == 2)
                {
                    // 小时输入错误，超出范围
                    XRVoice_PlayRaw(0x05, 0x00); // 播报"错误，请重新输入"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 25;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x07); // 播报"录入成功"
                }
                break;
            case 0x08: // 二十六
                if(g_voice_alarm_edit == 2)
                {
                    // 小时输入错误，超出范围
                    XRVoice_PlayRaw(0x05, 0x00); // 播报"错误，请重新输入"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 26;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x08); // 播报"录入成功"
                }
                break;
            case 0x09: // 二十七
                if(g_voice_alarm_edit == 2)
                {
                    // 小时输入错误，超出范围
                    XRVoice_PlayRaw(0x05, 0x00); // 播报"错误，请重新输入"
                }
                else if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 27;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x09); // 播报"录入成功"
                }
                break;
            case 0x10: // 二十八
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 28;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x10); // 播报"录入成功"
                }
                break;
            case 0x11: // 二十九
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 29;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x11); // 播报"录入成功"
                }
                break;
            case 0x12: // 三十
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 30;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x12); // 播报"录入成功"
                }
                break;
            case 0x13: // 三十一
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 31;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x13); // 播报"录入成功"
                }
                break;
            case 0x14: // 三十二
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 32;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x14); // 播报"录入成功"
                }
                break;
            case 0x15: // 三十三
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 33;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x15); // 播报"录入成功"
                }
                break;
            case 0x16: // 三十四
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 34;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x16); // 播报"录入成功"
                }
                break;
            case 0x17: // 三十五
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 35;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x17); // 播报"录入成功"
                }
                break;
            case 0x18: // 三十六
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 36;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x18); // 播报"录入成功"
                }
                break;
            case 0x19: // 三十七
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 37;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x19); // 播报"录入成功"
                }
                break;
            case 0x20: // 三十八
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 38;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x20); // 播报"录入成功"
                }
                break;
            case 0x21: // 三十九
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 39;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x21); // 播报"录入成功"
                }
                break;
            case 0x22: // 四十
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 40;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x22); // 播报"录入成功"
                }
                break;
            case 0x23: // 四十一
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 41;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x23); // 播报"录入成功"
                }
                break;
            case 0x24: // 四十二
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 42;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x24); // 播报"录入成功"
                }
                break;
            case 0x25: // 四十三
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 43;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x25); // 播报"录入成功"
                }
                break;
            case 0x26: // 四十四
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 44;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x26); // 播报"录入成功"
                }
                break;
            case 0x27: // 四十五
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 45;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x27); // 播报"录入成功"
                }
                break;
            case 0x28: // 四十六
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 46;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x28); // 播报"录入成功"
                }
                break;
            case 0x29: // 四十七
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 47;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x29); // 播报"录入成功"
                }
                break;
            case 0x30: // 四十八
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 48;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x30); // 播报"录入成功"
                }
                break;
            case 0x31: // 四十九
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 49;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x31); // 播报"录入成功"
                }
                break;
            case 0x32: // 五十
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 50;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x32); // 播报"录入成功"
                }
                break;
            case 0x33: // 五十一
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 51;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x33); // 播报"录入成功"
                }
                break;
            case 0x34: // 五十二
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 52;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x34); // 播报"录入成功"
                }
                break;
            case 0x35: // 五十三
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 53;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x35); // 播报"录入成功"
                }
                break;
            case 0x36: // 五十四
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 54;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x36); // 播报"录入成功"
                }
                break;
            case 0x37: // 五十五
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 55;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x37); // 播报"录入成功"
                }
                break;
            case 0x38: // 五十六
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 56;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x38); // 播报"录入成功"
                }
                break;
            case 0x39: // 五十七
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 57;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x39); // 播报"录入成功"
                }
                break;
            case 0x40: // 五十八
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 58;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x40); // 播报"录入成功"
                }
                break;
            case 0x41: // 五十九
                if(g_voice_alarm_edit == 3)
                {
                    g_voice_alarm_minute = 59;
                    UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                    g_voice_alarm_edit = 0; // 退出修改模式
                    XRVoice_PlayRaw(0x04, 0x41); // 播报"录入成功"
                }
                break;
            case 0x42: // 六十
                if(g_voice_alarm_edit == 3)
                {
                    // 分钟输入错误，超出范围
                    XRVoice_PlayRaw(0x05, 0x00); // 播报"错误，请重新输入"
                }
                break;
        }
        return;
    }
    
    // 处理Type=05命令
    if(cmd_type == 0x05)
    {
        switch(cmd_id)
        {
            case 0x00: // 错误
                break;
        }
        return;
    }
    
    // 处理Type=08命令
    if(cmd_type == 0x08)
    {
        switch(cmd_id)
        {
            case 0x00: // 现在是什么时候
            {
                uint8_t hour = current_hour;
                uint8_t play_cmd_type = 0x08;
                uint8_t play_cmd_id;
                
                if(hour < 6)
                {
                    play_cmd_id = 0x03;
                }
                else if(hour >= 6 && hour < 12)
                {
                    play_cmd_id = 0x04;
                }
                else if(hour >= 12 && hour < 14)
                {
                    play_cmd_id = 0x05;
                }
                else if(hour >= 14 && hour < 18)
                {
                    play_cmd_id = 0x06;
                }
                else if(hour >= 18 && hour < 20)
                {
                    play_cmd_id = 0x07;
                }
                else
                {
                    play_cmd_id = 0x08;
                }
                
                XRVoice_PlayRaw(play_cmd_type, play_cmd_id);
                break;
            }
            case 0x01: // 欢迎
                break;
            case 0x03: // 现在是凌晨
                break;
            case 0x04: // 现在是早上
                break;
            case 0x05: // 现在是中午
                break;
            case 0x06: // 现在是下午
                break;
            case 0x07: // 现在是傍晚
                break;
            case 0x08: // 现在是晚上
                break;
        }
        return;
    }
    
    // 处理Type=09命令
    if(cmd_type == 0x09)
    {
        switch(cmd_id)
        {
            case 0x00: // 空盒
                break;
            case 0x01: // 闹钟一
                break;
            case 0x02: // 闹钟二
                break;
            case 0x03: // 闹钟三
                break;
        }
        return;
    }
}
