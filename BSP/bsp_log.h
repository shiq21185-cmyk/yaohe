#ifndef __BSP_LOG_H
#define __BSP_LOG_H

#include "stm32f10x.h"
#include <stdint.h>

/* USART1 (PA9/PA10) is the HC-06 port in normal firmware, so logging is
 * disabled by default. Set BSP_LOG_ENABLE to 1 only when HC-06 is disconnected
 * and USART1 is intentionally used as a diagnostic output. */
#ifndef BSP_LOG_ENABLE
#define BSP_LOG_ENABLE 0
#endif
void     BSP_Log_Init(void);
void     BSP_Log_Puts(const char *s);
void     BSP_Log_Dec(uint32_t v);
void     BSP_Log_Hex(uint32_t v, uint8_t digits);
void     BSP_Log_ShowAddr(uint32_t v);

#endif
