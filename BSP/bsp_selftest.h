#ifndef __BSP_SELFTEST_H
#define __BSP_SELFTEST_H

#include "stm32f10x.h"
#include <stdint.h>

/* STM32F103C8T6 SRAM address window: 20KB. */
#define BSP_RAM_BASE        0x20000000UL
#define BSP_RAM_END         0x20005000UL

uint8_t BSP_SelfTest_Run(void);
uint8_t BSP_SelfTest_Failed(void);
uint32_t BSP_SelfTest_UpperOkBytes(void);
void    BSP_SelfTest_Report(void);

#endif
