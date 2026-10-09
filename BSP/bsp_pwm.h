#ifndef __BSP_PWM_H
#define __BSP_PWM_H

#include <stdint.h>

/* ==========================================================================
 * 舵机 PWM 层
 * --------------------------------------------------------------------------
 * 硬件细节全部收在这里：TIM2_CH2 部分重映射到 PB3，20 ms 周期、1 us 分辨率。
 * 注意 PB3/PB4 默认被 JTAG 占用，必须先关 JTAG 才能当 PWM 引脚用。
 * 上层只说"多少微秒的高电平脉宽"，不再出现任何 TIM 寄存器操作。
 * ========================================================================== */

#define BSP_PWM_PERIOD_US       20000U  /* 20ms / 50Hz，舵机标准周期 */
#define BSP_PWM_MIN_PULSE_US      500U  /* 对应 0 度 */
#define BSP_PWM_MAX_PULSE_US     2500U  /* 对应 180 度 */
#define BSP_PWM_DEFAULT_PULSE_US 1500U  /* 上电默认中位 */

/* 配置 TIM2_CH2 为 20ms 周期、1us 分辨率的舵机 PWM；上电调用一次。 */
void BSP_PWM_ServoInit(void);

/* 设置高电平脉宽（微秒），超出范围自动截断到 BSP_PWM_MIN/MAX_PULSE_US。 */
void BSP_PWM_SetPulseUs(uint16_t pulse_us);

#endif
