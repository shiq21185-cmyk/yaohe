#ifndef __BSP_SELFTEST_H
#define __BSP_SELFTEST_H

#include "stm32f10x.h"
#include <stdint.h>

/* Addressable SRAM on the STM32F103C8T6 used here. The part is a large die
 * marked as a C8, so the upper 28 KB above the 20 KB window has to be proven
 * before the linker is allowed to place data there. */
#define BSP_RAM_BASE        0x20000000UL
#define BSP_RAM_END         0x2000C000UL   /* 48 KB */

/* Region handed to the linker today (20 KB). Everything above is "new". */
#define BSP_RAM_OLD_END     0x20005000UL

/* Stack guard: the self-test starts above this address so the running stack
 * (MSP sits at BSP_RAM_OLD_END while main() executes) is never clobbered. */
#define BSP_RAM_TEST_BASE   (BSP_RAM_OLD_END - 0x800UL)

uint8_t BSP_SelfTest_Run(void);
uint8_t BSP_SelfTest_Failed(void);
uint32_t BSP_SelfTest_UpperOkBytes(void);
void    BSP_SelfTest_Report(void);

#endif
