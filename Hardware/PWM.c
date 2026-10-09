#include "stm32f10x.h"
#include "bsp_pwm.h"

/* PB3 - TIM2_CH2 舵机 PWM。
 * 时钟使能、GPIO 重映射、TIM 时基与 OC 配置全部收敛到 BSP/bsp_pwm.c，
 * 这里只保留对外的语义入口，本文件不再出现任何寄存器/时钟调用。 */

void PWM_Init(void)
{
    BSP_PWM_ServoInit();
}

void PWM_SetCompare2(uint16_t Compare)
{
    BSP_PWM_SetPulseUs(Compare);
}
