#include "bsp_uart.h"
#include "bsp_gpio.h"

/* ==========================================================================
 * UART 语义层实现
 * ========================================================================== */

/* 各口的硬件描述：把"哪个 USART、挂在哪条 APB 上、用哪两个引脚、
 * 中断优先级多少"集中在一张表里，驱动层不再重复这些细节。 */
typedef struct {
    USART_TypeDef *usart;
    uint32_t       clock;      /* RCC_APBxPeriph_USARTy */
    uint8_t        on_apb2;    /* 1 = APB2(USART1)，0 = APB1(USART2/3) */
    GPIO_TypeDef  *port;
    uint16_t       tx_pin;
    uint16_t       rx_pin;
    IRQn_Type      irqn;
    uint8_t        preempt;    /* 抢占优先级，与重构前完全一致 */
    uint8_t        sub;        /* 子优先级，与重构前完全一致 */
} BSP_UartHw_t;

static const BSP_UartHw_t s_hw[BSP_UART_COUNT] = {
    /* BT   */ { USART1, RCC_APB2Periph_USART1, 1, GPIOA, GPIO_Pin_9,  GPIO_Pin_10, USART1_IRQn, 3, 0 },
    /* ESP  */ { USART2, RCC_APB1Periph_USART2, 0, GPIOA, GPIO_Pin_2,  GPIO_Pin_3,  USART2_IRQn, 1, 1 },
    /* VOICE*/ { USART3, RCC_APB1Periph_USART3, 0, GPIOB, GPIO_Pin_10, GPIO_Pin_11, USART3_IRQn, 2, 0 }
};

/* BT 与 VOICE 口由本层提供环形缓冲。
 *
 * ESP 口故意留空（buf == 0）：ESP8266 的 AT 应答解析依赖 Hardware/esp8266.c
 * 里那个 1280 字节线性缓冲 + strstr，在没有实机复测之前不替换它。它的
 * USART2_IRQHandler 也只做"收字节入缓冲"，符合分层要求，只是缓冲由
 * 驱动自己持有。 */
static uint8_t s_bt_rx_buf[BSP_UART_BT_RX_SIZE];
static uint8_t s_voice_rx_buf[BSP_UART_VOICE_RX_SIZE];

static BSP_UartRx_t s_rx[BSP_UART_COUNT] = {
    { s_bt_rx_buf,    BSP_UART_BT_RX_SIZE,    0u, 0u, 0u },
    { 0,              0u,                     0u, 0u, 0u },
    { s_voice_rx_buf, BSP_UART_VOICE_RX_SIZE, 0u, 0u, 0u }
};

/* -------------------------------------------------------------------------- */

static void rx_push(BSP_UartRx_t *r, uint8_t b)
{
    uint16_t next = (uint16_t)((r->head + 1u) & (uint16_t)(r->size - 1u));

    if (next == r->tail)
    {
        /* 缓冲满：丢弃最新的字节并计数。丢最新而不是覆盖最旧，
         * 是为了不破坏已经排在前面的整帧数据。 */
        r->overrun++;
        return;
    }

    r->buf[r->head] = b;
    r->head = next;
}

static int rx_pop(BSP_UartRx_t *r)
{
    uint8_t b;

    if (r->head == r->tail) { return -1; }
    b = r->buf[r->tail];
    r->tail = (uint16_t)((r->tail + 1u) & (uint16_t)(r->size - 1u));
    return (int)b;
}

/* -------------------------------------------------------------------------- */

void BSP_UART_Init(BSP_UartId id, uint32_t baudrate)
{
    const BSP_UartHw_t *hw;
    USART_InitTypeDef   uart;
    NVIC_InitTypeDef    nvic;

    if (id >= BSP_UART_COUNT) { return; }
    hw = &s_hw[id];

    if (hw->on_apb2) { RCC_APB2PeriphClockCmd(hw->clock, ENABLE); }
    else             { RCC_APB1PeriphClockCmd(hw->clock, ENABLE); }

    /* TX 复用推挽、RX 浮空输入；GPIO 端口时钟由 BSP_GPIO 内部使能。 */
    BSP_GPIO_ConfigAFPP(hw->port, hw->tx_pin);
    BSP_GPIO_ConfigInFloat(hw->port, hw->rx_pin);

    uart.USART_BaudRate            = baudrate;
    uart.USART_WordLength          = USART_WordLength_8b;
    uart.USART_StopBits            = USART_StopBits_1;
    uart.USART_Parity              = USART_Parity_No;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    uart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(hw->usart, &uart);

    USART_ITConfig(hw->usart, USART_IT_RXNE, ENABLE);

    nvic.NVIC_IRQChannel                   = hw->irqn;
    nvic.NVIC_IRQChannelPreemptionPriority = hw->preempt;
    nvic.NVIC_IRQChannelSubPriority        = hw->sub;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    USART_Cmd(hw->usart, ENABLE);
}

/* -------------------------------------------------------------------------- */

void BSP_UART_SendByte(BSP_UartId id, uint8_t b)
{
    USART_TypeDef *u;

    if (id >= BSP_UART_COUNT) { return; }
    u = s_hw[id].usart;

    while ((u->SR & USART_FLAG_TXE) == 0) { }
    u->DR = b;
}

void BSP_UART_Send(BSP_UartId id, const uint8_t *data, uint16_t len)
{
    uint16_t i;

    if (data == 0) { return; }
    for (i = 0; i < len; i++) { BSP_UART_SendByte(id, data[i]); }
}

/* -------------------------------------------------------------------------- */

uint16_t BSP_UART_Available(BSP_UartId id)
{
    if (id >= BSP_UART_COUNT || s_rx[id].buf == 0) { return 0u; }
    return (uint16_t)((s_rx[id].head - s_rx[id].tail) & (uint16_t)(s_rx[id].size - 1u));
}

int BSP_UART_GetByte(BSP_UartId id)
{
    if (id >= BSP_UART_COUNT || s_rx[id].buf == 0) { return -1; }
    return rx_pop(&s_rx[id]);
}

uint16_t BSP_UART_Read(BSP_UartId id, uint8_t *dst, uint16_t maxlen)
{
    uint16_t n = 0;
    int b;

    if (dst == 0) { return 0u; }
    while (n < maxlen)
    {
        b = BSP_UART_GetByte(id);
        if (b < 0) { break; }
        dst[n++] = (uint8_t)b;
    }
    return n;
}

void BSP_UART_Flush(BSP_UartId id)
{
    if (id >= BSP_UART_COUNT || s_rx[id].buf == 0) { return; }
    s_rx[id].tail = s_rx[id].head;
}

uint16_t BSP_UART_Overrun(BSP_UartId id)
{
    if (id >= BSP_UART_COUNT || s_rx[id].buf == 0) { return 0u; }
    return s_rx[id].overrun;
}

/* -------------------------------------------------------------------------- */

int BSP_UART_IsrFetch(BSP_UartId id)
{
    USART_TypeDef *u;
    uint8_t        b;

    if (id >= BSP_UART_COUNT) { return -1; }
    u = s_hw[id].usart;

    if (USART_GetITStatus(u, USART_IT_RXNE) == RESET) { return -1; }

    /* 读 DR 同时清 RXNE；再清一次 pending 位，和重构前的写法保持一致。 */
    b = (uint8_t)u->DR;
    USART_ClearITPendingBit(u, USART_IT_RXNE);

    /* 由本层持有缓冲的口（BT / VOICE）直接入缓冲；
     * ESP 口的缓冲仍在驱动里，这里只把字节交回去。 */
    if (s_rx[id].buf != 0) { rx_push(&s_rx[id], b); }

    return (int)b;
}
