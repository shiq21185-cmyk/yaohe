#include "bsp_time.h"
#include "stm32f10x.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_rcc.h"

/* ==========================================================================
 * 时基层实现：TIM4 自由运行计数器
 * ========================================================================== */

#define BSP_TIME_TIMER          TIM4
#define BSP_TIME_TIMER_CLOCK    RCC_APB1Periph_TIM4
#define BSP_TIME_COUNTER_HZ     1000000UL   /* 目标计数频率 1 MHz */
#define BSP_TIME_MAX_US         65535UL     /* 16 位计数器的单次上限 */

void BSP_Time_Init(void)
{
    TIM_TimeBaseInitTypeDef init;

    RCC_APB1PeriphClockCmd(BSP_TIME_TIMER_CLOCK, ENABLE);
    TIM_InternalClockConfig(BSP_TIME_TIMER);

    init.TIM_ClockDivision     = TIM_CKD_DIV1;
    init.TIM_CounterMode       = TIM_CounterMode_Up;
    init.TIM_Period            = 0xFFFF;
    /* 预分频由 SystemCoreClock 推导，而不是写死 72-1，换主频时自动跟随。 */
    init.TIM_Prescaler         = (uint16_t)((SystemCoreClock / BSP_TIME_COUNTER_HZ) - 1U);
    init.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(BSP_TIME_TIMER, &init);
    TIM_Cmd(BSP_TIME_TIMER, ENABLE);
}

void BSP_DelayUs(uint32_t us)
{
    if (us > BSP_TIME_MAX_US) { us = BSP_TIME_MAX_US; }

    TIM_SetCounter(BSP_TIME_TIMER, 0);
    while (TIM_GetCounter(BSP_TIME_TIMER) < us) { }
}

void BSP_DelayMs(uint32_t ms)
{
    while (ms--) { BSP_DelayUs(1000U); }
}

void BSP_DelayS(uint32_t s)
{
    while (s--) { BSP_DelayMs(1000U); }
}

uint16_t BSP_Time_GetCounter(void)
{
    return (uint16_t)TIM_GetCounter(BSP_TIME_TIMER);
}
