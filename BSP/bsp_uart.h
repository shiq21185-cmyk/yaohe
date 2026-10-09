#ifndef __BSP_UART_H
#define __BSP_UART_H

#include "stm32f10x.h"
#include <stdint.h>

/* ==========================================================================
 * UART 语义层
 * --------------------------------------------------------------------------
 * 收敛三件事：
 *   1) USART 的时钟、GPIO 复用、波特率、NVIC 优先级配置；
 *   2) 阻塞式发送；
 *   3) 接收环形缓冲 —— ISR 的唯一职责是把 DR 里的字节 push 进缓冲，
 *      协议解析一律回到任务上下文，避免在中断里做 Flash 擦写、取信号量
 *      这类只能在线程态完成的事情。
 * ========================================================================== */

typedef enum {
    BSP_UART_BT = 0,     /* USART1   9600   PA9 (TX) / PA10(RX)   HC-06 蓝牙 */
    BSP_UART_ESP,        /* USART2  115200   PA2 (TX) / PA3 (RX)   ESP8266 */
    BSP_UART_VOICE,      /* USART3   9600   PB10(TX) / PB11(RX)  小R 语音 */
    BSP_UART_COUNT
} BSP_UartId;

/* 接收环形缓冲容量，必须是 2 的幂（实现用 & (size-1) 取模）。 */
#define BSP_UART_BT_RX_SIZE      64u
#define BSP_UART_VOICE_RX_SIZE   32u

/* 单生产者（ISR）/ 单消费者（任务）模型：head 只由 ISR 写，
 * tail 只由任务写，因此读写指针本身不需要临界区保护。 */
typedef struct {
    uint8_t          *buf;
    uint16_t          size;
    volatile uint16_t head;      /* ISR 递增 */
    volatile uint16_t tail;      /* 任务递增 */
    volatile uint16_t overrun;   /* 因缓冲满而丢弃的字节数，供自检观察 */
} BSP_UartRx_t;

/* 初始化：时钟 + GPIO + 参数 + NVIC + 打开 RXNE 中断。 */
void BSP_UART_Init(BSP_UartId id, uint32_t baudrate);

/* 阻塞式发送（等 TXE）。任务与中断上下文都可以调用。 */
void BSP_UART_SendByte(BSP_UartId id, uint8_t b);
void BSP_UART_Send(BSP_UartId id, const uint8_t *data, uint16_t len);

/* ---------- 接收侧：只能在任务上下文调用 ---------- */
uint16_t BSP_UART_Available(BSP_UartId id);
int      BSP_UART_GetByte(BSP_UartId id);                        /* 无数据返回 -1 */
uint16_t BSP_UART_Read(BSP_UartId id, uint8_t *dst, uint16_t maxlen);
void     BSP_UART_Flush(BSP_UartId id);
uint16_t BSP_UART_Overrun(BSP_UartId id);

/* 各口 IRQHandler 的唯一职责就是调用它：
 * 判 RXNE -> 读 DR（读 DR 顺带清标志）-> 该口若由本层持有缓冲则顺便入缓冲。
 * 返回值：收到的字节 0~255；本次中断没有数据时返回 -1。
 * 这样"哪个 USART 的哪个标志位"这类寄存器细节也留在 BSP 层里。 */
int BSP_UART_IsrFetch(BSP_UartId id);

#endif
