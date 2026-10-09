#include "stm32f10x.h"
#include "bsp_pwm.h"
#include "bsp_gpio.h"

/* TIM2_CH2 部分重映射到 PB3 驱动舵机：20 ms 周期、1 MHz 计数。
 * 注意 PB3/PB4 默认被 JTAG 占用，必须先关掉 JTAG 才能当普通 IO/PWM 用，
 * 所以这里的调用顺序不能改：AFIO 时钟 -> 重映射 -> 配置引脚。 */
void BSP_PWM_ServoInit(void)
{
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_OCInitTypeDef       TIM_OCInitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);

    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
    GPIO_PinRemapConfig(GPIO_PartialRemap1_TIM2, ENABLE);

    /* 复用推挽输出（含 GPIOB 的 APB2 时钟使能）。 */
    BSP_GPIO_ConfigAFPP(GPIOB, GPIO_Pin_3);

    TIM_InternalClockConfig(TIM2);

    TIM_TimeBaseInitStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode       = TIM_CounterMode_Up;
    /* 1us 计数：预分频按当前主频算，不再写死 71，换主频也不会偏。 */
    TIM_TimeBaseInitStructure.TIM_Prescaler         = (uint16_t)((SystemCoreClock / 1000000U) - 1U);
    TIM_TimeBaseInitStructure.TIM_Period            = BSP_PWM_PERIOD_US - 1U;
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM2, &TIM_TimeBaseInitStructure);

    TIM_OCStructInit(&TIM_OCInitStructure);
    TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse       = BSP_PWM_DEFAULT_PULSE_US;
    TIM_OC2Init(TIM2, &TIM_OCInitStructure);

    TIM_OC2PreloadConfig(TIM2, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM2, ENABLE);

    TIM_Cmd(TIM2, ENABLE);
}

/* 设置高电平脉宽（微秒），超出范围自动截断。 */
void BSP_PWM_SetPulseUs(uint16_t pulse_us)
{
    if(pulse_us < BSP_PWM_MIN_PULSE_US) { pulse_us = BSP_PWM_MIN_PULSE_US; }
    if(pulse_us > BSP_PWM_MAX_PULSE_US) { pulse_us = BSP_PWM_MAX_PULSE_US; }

    TIM_SetCompare2(TIM2, pulse_us);
}
