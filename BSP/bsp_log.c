#include "bsp_log.h"
#include "bsp_gpio.h"

/* ------------------------------------------------------------------ *
 * Register-level TX on USART1. No library call, no tick dependency.  *
 * ------------------------------------------------------------------ */

void BSP_Log_Init(void)
{
    USART_InitTypeDef uart;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    /* PA9 = TX，复用推挽输出；PA9 的 GPIOA 时钟由 BSP_GPIO 内部使能。
     * 这里刻意不调用 BSP_UART_*：日志口必须早于 BSP_UART 自己可用，
     * 而且要能在故障上下文里直接读写 DR/SR。 */
    BSP_GPIO_ConfigAFPP(GPIOA, GPIO_Pin_9);

    /* Leave PA10 floating until the real HC-06 driver takes over (it enables RXNE). */
    uart.USART_BaudRate            = 9600;
    uart.USART_WordLength          = USART_WordLength_8b;
    uart.USART_StopBits            = USART_StopBits_1;
    uart.USART_Parity              = USART_Parity_No;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode                = USART_Mode_Tx;
    USART_Init(USART1, &uart);
    USART_Cmd(USART1, ENABLE);
}

void BSP_Log_Putc(char c)
{
    while ((USART1->SR & USART_FLAG_TXE) == 0) { }
    USART1->DR = (uint8_t)c;
}

void BSP_Log_Puts(const char *s)
{
    while (*s) { BSP_Log_Putc(*s++); }
}

void BSP_Log_Dec(uint32_t v)
{
    char buf[11];
    uint8_t n = 0;

    if (v == 0) { BSP_Log_Putc('0'); return; }
    while (v) { buf[n++] = (char)('0' + (v % 10u)); v /= 10u; }
    while (n) { BSP_Log_Putc(buf[--n]); }
}

void BSP_Log_Hex(uint32_t v, uint8_t digits)
{
    char c;

    while (digits) {
        digits--;
        c = (char)((v >> (digits * 4)) & 0xFu);
        BSP_Log_Putc((c < 10) ? (char)('0' + c) : (char)('A' + c - 10));
    }
}

void BSP_Log_ShowAddr(uint32_t v)
{
    BSP_Log_Putc('0');
    BSP_Log_Putc('x');
    BSP_Log_Hex(v, 8);
}
