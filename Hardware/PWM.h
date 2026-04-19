#ifndef __PWM_H
#define __PWM_H

#include "stm32f10x.h"

// PB3 - TIM2_CH2 ¶æ»úPWM
void PWM_Init(void);
void PWM_SetCompare2(uint16_t Compare);

#endif
