#include "xrvoice.h"
#include "string.h"
#include "Delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include "OLED.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"
#include <stdio.h>

// ==================== 全局变量 ====================
static uint8_t uart3_rx_buffer[XRVOICE_RX_BUFFER_SIZE];
static volatile uint8_t uart3_rx_index = 0;
static volatile uint8_t uart3_rx_complete = 0;

static volatile uint8_t g_sleep_playing = 0;
// 指令去抖
static volatile uint32_t valid_frame_count = 0;
static volatile uint32_t last_cmd_time = 0;
static volatile uint8_t last_cmd_type = 0;
static volatile uint8_t last_cmd_id = 0;

// 唤醒状态
static volatile uint8_t wakeup_state = 0;
static volatile uint32_t wakeup_time = 0;

// 回调函数
static VoiceCommandCallback_t voice_callback = NULL;

// ==================== 自环过滤功能 ====================
static volatile uint8_t g_self_sent = 0;           // 是否是自己发送的指令
static volatile uint32_t g_last_send_tick = 0;     // 上次发送时间
static volatile uint8_t g_last_sent_type = 0;      // 上次发送的指令类型
static volatile uint8_t g_last_sent_id = 0;        // 上次发送的指令ID

// 播放状态变量
static volatile uint8_t g_voice_playing = 0;       // 语音正在播放
static volatile uint32_t g_last_play_tick = 0;     // 上次播放时间
static volatile uint8_t g_last_play_cmd_type = 0;  // 上次播放的指令类型
static volatile uint8_t g_last_play_cmd_id = 0;    // 上次播放的指令ID

// 唤醒超时时间
#define WAKEUP_TIMEOUT_MS  30000
// 自环过滤时间- 用于过滤自己发送的指令回显
#define SELF_LOOP_FILTER_MS 300
// 语音播放去抖时间
#define VOICE_PLAY_DEBOUNCE_MS 1000

// ==================== 内部函数 ====================

static void XRVoice_Send(uint8_t* data, uint8_t len)
{
    BSP_UART_Send(BSP_UART_VOICE, data, len);
}

static void CheckWakeupTimeout(void)
{
    // 闹钟响铃、空盒报警或正在播放语音时，不触发唤醒超时，保持唤醒状态避免打断播报
    extern volatile uint8_t g_alarm_ringing;
    extern volatile uint8_t g_empty_box_alarm_active;

    if(wakeup_state &&
       !g_alarm_ringing &&
       !g_empty_box_alarm_active &&
       !g_voice_playing &&
       (xTaskGetTickCount() - wakeup_time) > pdMS_TO_TICKS(WAKEUP_TIMEOUT_MS))
    {
        wakeup_state = 0;
    }
}

// ==================== 判断是否为自环回显 ====================
static uint8_t IsSelfLoopback(uint8_t cmd_type, uint8_t cmd_id)
{
    uint32_t now = xTaskGetTickCount();

    // 判断是自己发送的指令且在过滤时间范围内，则认为是自环回显
    if(g_self_sent &&
       cmd_type == g_last_sent_type &&
       cmd_id == g_last_sent_id &&
       (now - g_last_send_tick) < pdMS_TO_TICKS(SELF_LOOP_FILTER_MS))
    {
        return 1;
    }

    return 0;
}

// ==================== 记录自己发送的指令 ====================
static void RecordSelfSent(uint8_t cmd_type, uint8_t cmd_id)
{
    g_self_sent = 1;
    g_last_sent_type = cmd_type;
    g_last_sent_id = cmd_id;
    g_last_send_tick = xTaskGetTickCount();
}

// ==================== 检查自环发送超时 ====================
static void CheckSelfSentTimeout(void)
{
    if(g_self_sent &&
       (xTaskGetTickCount() - g_last_send_tick) > pdMS_TO_TICKS(SELF_LOOP_FILTER_MS))
    {
        g_self_sent = 0;
    }
}

static void XRVoice_ParseCommand(uint8_t cmd_type, uint8_t cmd_id)
{
    uint32_t current = xTaskGetTickCount();

    // 100ms内相同指令去抖
    if(cmd_type == last_cmd_type && cmd_id == last_cmd_id &&
       (current - last_cmd_time) < pdMS_TO_TICKS(100)) {
        return;
    }

    last_cmd_type = cmd_type;
    last_cmd_id = cmd_id;
    last_cmd_time = current;
    valid_frame_count++;

    CheckWakeupTimeout();

    // 处理Type=01, ID=02
    if(cmd_type == 0x01 && cmd_id == 0x02)
    {
        wakeup_state = 1;
        wakeup_time = current;
    }

    // 休息指令特殊处理：闹钟响铃、空盒报警或正在播放语音时，忽略休息指令避免打断播报
    extern volatile uint8_t g_alarm_ringing;
    extern volatile uint8_t g_empty_box_alarm_active;
    if(cmd_type == 0x01 && cmd_id == 0x01)
    {
        // 只有在没有闹钟、没有空盒报警、也没有正在播放语音时才进入休眠
        // 避免播放语音时自己发送的停止指令回显被误判为用户休息指令
        if(!g_alarm_ringing && !g_empty_box_alarm_active && !g_voice_playing) {
            wakeup_state = 0;
        }
    }
    else
    {
        // 如果没有唤醒就返回
        if(!wakeup_state)
        {
            return;
        }
        wakeup_time = current;
    }

    // 回调函数处理收到的指令ID
    if(voice_callback)
    {
        voice_callback(cmd_type, cmd_id);
    }
}

// ==================== 语音模块初始化 ====================

void XRVoice_Init(VoiceCommandCallback_t callback)
{
    voice_callback = callback;

    /* USART3（PB10/TX、PB11/RX）的时钟、GPIO 复用、9600 波特率与中断优先级
     * 全部由 BSP_UART 统一配置，本层只负责组帧与语义解析。 */
    BSP_UART_Init(BSP_UART_VOICE, 9600U);

    // 初始化接收缓冲区和状态变量
    memset((uint8_t*)uart3_rx_buffer, 0, XRVOICE_RX_BUFFER_SIZE);
    uart3_rx_index = 0;
    uart3_rx_complete = 0;
    wakeup_state = 0;
    wakeup_time = 0;

    // 初始化指令统计变量
    valid_frame_count = 0;
    last_cmd_time = 0;
    last_cmd_type = 0;
    last_cmd_id = 0;

    // 初始化自环和播放状态
    g_self_sent = 0;
    g_voice_playing = 0;
    g_last_play_cmd_type = 0;
}

void XRVoice_Task(void)
{
    /* USART3 中断只把字节推进 BSP_UART 环形缓冲，5 字节组帧在这里完成。
     * 本函数由 Voice_Task 以 10ms 周期调用，而 5 字节帧 @9600bps 约 5.2ms
     * 发完，因此不会漏字节。 */
    {
        int b;
        while((b = BSP_UART_GetByte(BSP_UART_VOICE)) >= 0)
        {
            if(uart3_rx_index < XRVOICE_RX_BUFFER_SIZE)
            {
                uart3_rx_buffer[uart3_rx_index++] = (uint8_t)b;

                // 接收到5个字节表示一帧指令接收完成
                if(uart3_rx_index == 5)
                {
                    uart3_rx_complete = 1;
                    break;      /* 先把这一帧处理掉，剩余字节下一轮再取 */
                }
            }
            else
            {
                // 缓冲区溢出，重置接收
                uart3_rx_index = 0;
                memset((uint8_t*)uart3_rx_buffer, 0, XRVOICE_RX_BUFFER_SIZE);
            }
        }
    }

    if(uart3_rx_complete)
    {
        // 检查是否是合法的语音指令帧 (AA 55 cmd_type cmd_id FB)
        if(uart3_rx_index == 5 &&
           uart3_rx_buffer[0] == 0xAA &&
           uart3_rx_buffer[1] == 0x55 &&
           uart3_rx_buffer[4] == 0xFB)
        {
            uint8_t cmd_type = uart3_rx_buffer[2];
            uint8_t cmd_id = uart3_rx_buffer[3];

            // 如果不是自环回显则处理指令
            if(!IsSelfLoopback(cmd_type, cmd_id))
            {
                XRVoice_ParseCommand(cmd_type, cmd_id);
            }
        }

        // 清空缓冲区准备下一次接收
        memset((uint8_t*)uart3_rx_buffer, 0, XRVOICE_RX_BUFFER_SIZE);
        uart3_rx_index = 0;
        uart3_rx_complete = 0;
    }

    // 检查自环超时
    CheckSelfSentTimeout();
}

uint8_t XRVoice_IsWakeup(void)
{
    return wakeup_state;
}

void XRVoice_Wakeup(void)
{
    wakeup_state = 1;
    wakeup_time = xTaskGetTickCount();
}

void XRVoice_Play(uint8_t index)
{
    if(index < 1 || index > 9) return;

    // 索引对应的语音指令映射
    // 索引1-3: 欢迎语/休息语/唤醒语 (Type=01, ID=00-02)
    // 索引4: 取药提醒 (Type=08, ID=1E)
    // 索引5-7: 闹钟1-3 (Type=09, ID=00-02)
    // 索引8-9: 预留

    uint8_t cmd_type, cmd_id;

    switch(index)
    {
        case 1:  // 欢迎语
            cmd_type = 0x01; cmd_id = 0x00; break;
        case 2:  // 休息语
            cmd_type = 0x01; cmd_id = 0x01; break;
        case 3:  // 唤醒语
            cmd_type = 0x01; cmd_id = 0x02; break;
        case 4:  // 取药提醒
            cmd_type = 0x08; cmd_id = 0x1E; break;
        case 5:  // 闹钟1提醒
            cmd_type = 0x09; cmd_id = 0x00; break;
        case 6:  // 闹钟2提醒
            cmd_type = 0x09; cmd_id = 0x01; break;
        case 7:  // 闹钟3提醒
            cmd_type = 0x09; cmd_id = 0x02; break;
        case 8:  // 预留
        case 9:  // 预留
        default:
            return;
    }

    XRVoice_PlayRaw(cmd_type, cmd_id);
}

void XRVoice_PlayRaw(uint8_t cmd_type, uint8_t cmd_id)
{
    uint32_t now = xTaskGetTickCount();

    // 如果正在播放相同语音且在去抖时间内，避免重复播放
    if(g_voice_playing &&
       g_last_play_cmd_type == cmd_type &&
       g_last_play_cmd_id == cmd_id &&
       (now - g_last_play_tick) < pdMS_TO_TICKS(VOICE_PLAY_DEBOUNCE_MS))
    {
        return;
    }

    // 如果正在播放其他语音，先停止
    if(g_voice_playing)
    {
        XRVoice_Stop();
        vTaskDelay(pdMS_TO_TICKS(20));
    }

    // 更新播放状态
    g_voice_playing = 1;
    g_last_play_cmd_type = cmd_type;
    g_last_play_cmd_id = cmd_id;
    g_last_play_tick = now;

    // 构造并发送语音播放指令 AA 55 cmd_type cmd_id FB
    uint8_t play_cmd[] = {0xAA, 0x55, cmd_type, cmd_id, 0xFB};
    RecordSelfSent(cmd_type, cmd_id);
    XRVoice_Send(play_cmd, sizeof(play_cmd));
}

// ==================== 音量控制函数 ====================
void XRVoice_VolumeUp(void)
{
    // 增大音量，不主动播放语音
}

void XRVoice_VolumeDown(void)
{
    // 减小音量，不主动播放语音
}

void XRVoice_VolumeMax(void)
{
    // 设置最大音量，不主动播放语音
}

void XRVoice_VolumeMid(void)
{
    // 设置中等音量，不主动播放语音
}

void XRVoice_VolumeMin(void)
{
    // 设置最小音量，不主动播放语音
}

// 设置音量 (1-6)
void XRVoice_SetVolume(uint8_t volume)
{
    if(volume < 1) volume = 1;
    if(volume > 6) volume = 6;

    // 发送音量设置指令：AA 55 0x06 音量值 FB
    uint8_t cmd[5] = {0xAA, 0x55, 0x06, volume, 0xFB};
    /* 音量指令必须发回语音模块自己的串口（USART3）。
     * 原实现误发到 USART1（HC-06 蓝牙口），导致音量设置从未生效。 */
    BSP_UART_Send(BSP_UART_VOICE, cmd, 5);
}

// 停止播放
void XRVoice_Stop(void)
{
    // 暂不发送停止指令，避免0x01 0x01指令播放"我去休息了"
    // 直接重置播放状态即可，语音模块播放完当前片段会自动停止
    g_voice_playing = 0;
    g_last_play_cmd_type = 0;
    g_last_play_cmd_id = 0;
}

// ==================== USART3中断服务函数 ====================
// 中断里只做一件事：把收到的字节推进 BSP_UART 的环形缓冲。
// 组帧与指令解析全部交给 XRVoice_Task() 在任务上下文完成。
void USART3_IRQHandler(void)
{
    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        BSP_UART_RxIsr(BSP_UART_VOICE);
        USART_ClearITPendingBit(USART3, USART_IT_RXNE);
    }
}
