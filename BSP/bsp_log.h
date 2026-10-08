#ifndef __BSP_LOG_H
#define __BSP_LOG_H

#include "stm32f10x.h"
#include <stdint.h>

/* BSP_Log is a blocking, register-level logger on USART1 (PA9 TX).
 * It deliberately does NOT use printf / USART_SendData / SysTick, so it also
 * works before the FreeRTOS scheduler starts and inside fault context.
 * Only ASCII is emitted: strings are never localized. */
void     BSP_Log_Init(void);
void     BSP_Log_Puts(const char *s);
void     BSP_Log_Dec(uint32_t v);
void     BSP_Log_Hex(uint32_t v, uint8_t digits);
void     BSP_Log_ShowAddr(uint32_t v);

#endif
