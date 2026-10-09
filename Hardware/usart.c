#include "usart.h"
#include "stm32f10x_usart.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"
#include "bsp_gpio.h"
#include "bsp_uart.h"

// ---------------------------------------------------------------------------
// 两个 USART 的时钟、GPIO 复用、波特率、接收中断优先级全部交给 BSP_UART 统一
// 配置，这里只保留"哪个口 = 什么用途"的语义对应关系。
// ---------------------------------------------------------------------------

// USART2 初始化 - ESP8266 使用（115200）
void USART2_Init(uint32_t baudrate)
{
    BSP_UART_Init(BSP_UART_ESP, baudrate);
}

// USART1初始化 - HC06使用（9600，波特率由调用方决定，不再写死）
void USART1_Init(uint32_t baudrate)
{
    BSP_UART_Init(BSP_UART_BT, baudrate);
}

// 中断里只做一件事：把收到的字节推进 BSP_UART 的环形缓冲。
// 协议解析（HC06_ProcessByte）与闹钟 Flash 擦写改由任务上下文的 HC06_Poll()
// 完成，中断里不再出现 taskENTER_CRITICAL() / FLASH_ErasePage。
void USART1_IRQHandler(void)
{
    if(USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)
    {
        BSP_UART_RxIsr(BSP_UART_BT);
        USART_ClearITPendingBit(USART1, USART_IT_RXNE);
    }
}
