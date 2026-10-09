#include "voice_command_handler.h"
#include "xrvoice.h"
#include "OLED.h"
#include "app_tasks.h"
#include "Servo.h"
#include "LED.h"


extern void Voice_Play_Current_Time(void);
extern void Voice_Play_Current_Time_With_Period(void);


extern volatile uint8_t g_alarm_ringing;
extern volatile uint8_t g_light_changed;
extern volatile uint8_t g_empty_box_alarm_active;
extern volatile uint8_t g_stop_empty_box_alarm;
extern volatile uint8_t g_volume;
extern volatile uint8_t display_mode;


volatile uint8_t g_voice_alarm_edit = 0;
volatile uint8_t g_voice_alarm_index = 0;
volatile uint8_t g_voice_alarm_hour = 0;
volatile uint8_t g_voice_alarm_minute = 0;


/* 在 OLED 上显示三组闹钟。 */
void RefreshAlarmDisplay(void)
{

    OLED_Lock();

    OLED_Clear();

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

    OLED_Unlock();
}


/* 校验并更新指定闹钟的时间。 */
void UpdateAlarmTime(uint8_t index, uint8_t hour, uint8_t minute)
{

    if(index >= 1 && index <= 3 && hour <= 23 && minute <= 59)
    {

        uint8_t alarm_index = index - 1;
        HC06_SetAlarm(alarm_index, hour, minute, g_bt_alarms[alarm_index].enabled);


        RefreshAlarmDisplay();


        XRVoice_PlayRaw(0x10, 0x05);
    }
}


/* cmd_type = 0x01（原 if 链第 1 个分支，函数体原样搬运） */
static void VoiceCmd_Type01(uint8_t cmd_id)
{
    switch(cmd_id)
    {
        case 0x00:
            break;
        case 0x01:
            break;
        case 0x02:
            break;
        case 0x03:
            if(g_volume < 6) {
                g_volume++;
                XRVoice_SetVolume(g_volume);
            }
            break;
        case 0x04:
            if(g_volume > 1) {
                g_volume--;
                XRVoice_SetVolume(g_volume);
            }
            break;
        case 0x05:
            g_volume = 6;
            XRVoice_SetVolume(g_volume);
            break;
        case 0x06:
            g_volume = 3;
            XRVoice_SetVolume(g_volume);
            break;
        case 0x07:
            g_volume = 1;
            XRVoice_SetVolume(g_volume);
            break;
        case 0x08:
            break;
        case 0x09:
            break;
    }
}

/* cmd_type = 0x08（原 if 链第 2 个分支，函数体原样搬运） */
static void VoiceCmd_Type08(uint8_t cmd_id)
{
    switch(cmd_id)
    {
        case 0x00:
            Voice_Play_Current_Time_With_Period();
            break;
    }
}

/* cmd_type = 0x10（原 if 链第 3 个分支，函数体原样搬运） */
static void VoiceCmd_Type10(uint8_t cmd_id)
{
    switch(cmd_id)
    {
        case 0x00:

            break;
    }
}

/* cmd_type = 0x02（原 if 链第 4 个分支，函数体原样搬运） */
static void VoiceCmd_Type02(uint8_t cmd_id)
{
    switch(cmd_id)
    {
        case 0x00:
            break;
        case 0x01:
            break;
        case 0x02:
            Servo_Open();
            break;
        case 0x03:
            Servo_Close();


            if(g_alarm_ringing)
            {
                g_alarm_ringing = 0;
                g_light_changed = 0;
                LED0_OFF();

                if(display_mode == 0) {
                    OLED_Lock();
                    OLED_ShowString(4, 1, "Medicine taken! ");
                    OLED_Unlock();
                }
            }


            if(g_empty_box_alarm_active)
            {
                g_stop_empty_box_alarm = 1;
                LED0_OFF();
            }
            break;
        case 0x04:
            g_voice_alarm_edit = 1;
            display_mode = 2;

            break;
        case 0x05:
            if(g_voice_alarm_edit == 1)
            {
                g_voice_alarm_index = 1;
                g_voice_alarm_edit = 2;
                XRVoice_PlayRaw(0x02, 0x05);
            }
            break;
        case 0x06:
            if(g_voice_alarm_edit == 1)
            {
                g_voice_alarm_index = 2;
                g_voice_alarm_edit = 2;
                XRVoice_PlayRaw(0x02, 0x06);
            }
            break;
        case 0x07:
            if(g_voice_alarm_edit == 1)
            {
                g_voice_alarm_index = 3;
                g_voice_alarm_edit = 2;
                XRVoice_PlayRaw(0x02, 0x07);
            }
            break;
        case 0x08:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 0;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x02, 0x08);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 0;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x02, 0x08);
            }
            break;
        case 0x09:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 1;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x02, 0x09);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 1;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x02, 0x09);
            }
            break;
        case 0x0A:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 2;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x02, 0x0A);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 2;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x02, 0x0A);
            }
            break;
        case 0x0B:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 3;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x02, 0x0B);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 3;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x02, 0x0B);
            }
            break;
        case 0x0C:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 4;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x02, 0x0C);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 4;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x02, 0x0C);
            }
            break;
    }
}

/* cmd_type = 0x03（原 if 链第 5 个分支，函数体原样搬运） */
static void VoiceCmd_Type03(uint8_t cmd_id)
{
    switch(cmd_id)
    {
        case 0x00:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 5;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x00);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 5;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x00);
            }
            break;
        case 0x01:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 6;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x01);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 6;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x01);
            }
            break;
        case 0x02:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 7;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x02);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 7;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x02);
            }
            break;
        case 0x03:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 8;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x03);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 8;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x03);
            }
            break;
        case 0x04:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 9;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x04);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 9;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x04);
            }
            break;
        case 0x05:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 10;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x05);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 10;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x05);
            }
            break;
        case 0x06:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 11;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x06);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 11;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x06);
            }
            break;
        case 0x07:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 12;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x07);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 12;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x07);
            }
            break;
        case 0x08:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 13;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x08);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 13;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x08);
            }
            break;
        case 0x09:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 14;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x09);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 14;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x09);
            }
            break;
        case 0x0A:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 15;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x0A);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 15;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x0A);
            }
            break;
        case 0x0B:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 16;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x0B);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 16;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x0B);
            }
            break;
        case 0x0C:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 17;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x03, 0x0C);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 17;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x03, 0x0C);
            }
            break;
    }
}

/* cmd_type = 0x04（原 if 链第 6 个分支，函数体原样搬运） */
static void VoiceCmd_Type04(uint8_t cmd_id)
{

    if(g_voice_alarm_edit == 2 && cmd_id >= 0x06) {

        XRVoice_PlayRaw(0x05, 0x00);
        return;
    }

    switch(cmd_id)
    {
        case 0x00:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 18;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x04, 0x00);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 18;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x00);
            }
            break;
        case 0x01:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 19;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x04, 0x01);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 19;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x01);
            }
            break;
        case 0x02:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 20;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x04, 0x02);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 20;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x02);
            }
            break;
        case 0x03:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 21;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x04, 0x03);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 21;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x03);
            }
            break;
        case 0x04:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 22;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x04, 0x04);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 22;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x04);
            }
            break;
        case 0x05:
            if(g_voice_alarm_edit == 2)
            {
                g_voice_alarm_hour = 23;

                uint8_t alarm_index = g_voice_alarm_index - 1;
                if(alarm_index < 3) {
                    g_bt_alarms[alarm_index].hour = g_voice_alarm_hour;

                    RefreshAlarmDisplay();
                }
                g_voice_alarm_edit = 3;
                XRVoice_PlayRaw(0x04, 0x05);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 23;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x05);
            }
            break;
        case 0x06:
            if(g_voice_alarm_edit == 2)
            {

                XRVoice_PlayRaw(0x05, 0x00);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 24;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x06);
            }
            break;

        case 0x07:
            if(g_voice_alarm_edit == 2)
            {

                XRVoice_PlayRaw(0x05, 0x00);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 25;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x07);
            }
            break;
        case 0x08:
            if(g_voice_alarm_edit == 2)
            {

                XRVoice_PlayRaw(0x05, 0x00);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 26;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x08);
            }
            break;
        case 0x09:
            if(g_voice_alarm_edit == 2)
            {

                XRVoice_PlayRaw(0x05, 0x00);
            }
            else if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 27;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x09);
            }
            break;
        case 0x10:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 28;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x10);
            }
            break;
        case 0x11:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 29;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x11);
            }
            break;
        case 0x12:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 30;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x12);
            }
            break;
        case 0x13:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 31;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x13);
            }
            break;
        case 0x14:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 32;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x14);
            }
            break;
        case 0x15:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 33;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x15);
            }
            break;
        case 0x16:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 34;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x16);
            }
            break;
        case 0x17:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 35;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x17);
            }
            break;
        case 0x18:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 36;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x18);
            }
            break;
        case 0x19:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 37;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x19);
            }
            break;
        case 0x20:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 38;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x20);
            }
            break;
        case 0x21:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 39;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x21);
            }
            break;
        case 0x22:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 40;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x22);
            }
            break;
        case 0x23:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 41;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x23);
            }
            break;
        case 0x24:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 42;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x24);
            }
            break;
        case 0x25:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 43;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x25);
            }
            break;
        case 0x26:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 44;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x26);
            }
            break;
        case 0x27:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 45;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x27);
            }
            break;
        case 0x28:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 46;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x28);
            }
            break;
        case 0x29:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 47;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x29);
            }
            break;
        case 0x30:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 48;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x30);
            }
            break;
        case 0x31:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 49;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x31);
            }
            break;
        case 0x32:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 50;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x32);
            }
            break;
        case 0x33:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 51;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x33);
            }
            break;
        case 0x34:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 52;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x34);
            }
            break;
        case 0x35:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 53;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x35);
            }
            break;
        case 0x36:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 54;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x36);
            }
            break;
        case 0x37:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 55;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x37);
            }
            break;
        case 0x38:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 56;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x38);
            }
            break;
        case 0x39:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 57;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x39);
            }
            break;
        case 0x40:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 58;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x40);
            }
            break;
        case 0x41:
            if(g_voice_alarm_edit == 3)
            {
                g_voice_alarm_minute = 59;
                UpdateAlarmTime(g_voice_alarm_index, g_voice_alarm_hour, g_voice_alarm_minute);
                g_voice_alarm_edit = 0;
                XRVoice_PlayRaw(0x04, 0x41);
            }
            break;
        case 0x42:
            if(g_voice_alarm_edit == 3)
            {

                XRVoice_PlayRaw(0x05, 0x00);
            }
            break;
    }
}

/* cmd_type = 0x05（原 if 链第 7 个分支，函数体原样搬运） */
static void VoiceCmd_Type05(uint8_t cmd_id)
{
    switch(cmd_id)
    {
        case 0x00:
            break;
    }
}

/* cmd_type = 0x08（原 if 链第 8 个分支，函数体原样搬运） */
static void VoiceCmd_Type08_2(uint8_t cmd_id)
{
    switch(cmd_id)
    {
        case 0x00:
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
        case 0x01:
            break;
        case 0x03:
            break;
        case 0x04:
            break;
        case 0x05:
            break;
        case 0x06:
            break;
        case 0x07:
            break;
        case 0x08:
            break;
    }
}

/* cmd_type = 0x09（原 if 链第 9 个分支，函数体原样搬运） */
static void VoiceCmd_Type09(uint8_t cmd_id)
{
    switch(cmd_id)
    {
        case 0x00:
            break;
        case 0x01:
            break;
        case 0x02:
            break;
        case 0x03:
            break;
    }
}

/* 语音命令分发表：以 cmd_type 为键，按上面的登记顺序线性查找，
 * 命中即调用对应处理函数并立即 return。
 * 这与原来 if 链的语义完全一致——原 if 链每个分支末尾都是 return，
 * 所以同一个 cmd_type 只有第一个匹配的分支会被执行。
 * 注意 0x08 在原文件里登记了两次：第二条同样不可达（被第一条的 return
 * 挡住），这里按原顺序原样保留登记，既不改动任何行为，
 * 也避免编译器报“函数已定义但未引用”。 */
typedef void (*VoiceCmdHandler_t)(uint8_t cmd_id);

typedef struct
{
    uint8_t           cmd_type;
    VoiceCmdHandler_t handler;
} VoiceCmdEntry_t;

static const VoiceCmdEntry_t g_voice_cmd_table[] =
{
    { 0x01, VoiceCmd_Type01 },
    { 0x08, VoiceCmd_Type08 },
    { 0x10, VoiceCmd_Type10 },
    { 0x02, VoiceCmd_Type02 },
    { 0x03, VoiceCmd_Type03 },
    { 0x04, VoiceCmd_Type04 },
    { 0x05, VoiceCmd_Type05 },
    { 0x08, VoiceCmd_Type08_2 },
    { 0x09, VoiceCmd_Type09 },
};

#define VOICE_CMD_TABLE_SIZE  (sizeof(g_voice_cmd_table) / sizeof(g_voice_cmd_table[0]))

/* 根据语音命令类型和编号执行对应操作。 */
void HandleVoiceCommand(uint8_t cmd_type, uint8_t cmd_id)
{
    unsigned int i;

    for(i = 0; i < VOICE_CMD_TABLE_SIZE; i++)
    {
        if(g_voice_cmd_table[i].cmd_type == cmd_type)
        {
            g_voice_cmd_table[i].handler(cmd_id);
            return;
        }
    }
}
