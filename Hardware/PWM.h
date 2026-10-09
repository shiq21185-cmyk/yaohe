#ifndef __PWM_H
#define __PWM_H

#include "stm32f10x.h"

// PB3 - TIM2_CH2 舵机 PWM（寄存器配置在 BSP/bsp_pwm.c）
void PWM_Init(void);
void PWM_SetCompare2(uint16_t Compare);   // 单位：us，自动截断到 500~2500

#endif
