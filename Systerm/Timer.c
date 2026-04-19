#include "stm32f10x.h"                  // Device header

// ==================== TIM4 用于延时函数 ====================

/**
  * @brief  TIM4初始化，用于Delay_us和Delay_ms
  * @param  无
  * @retval 无
  * @note   TIM4时钟72MHz，预分频72，计数频率1MHz（1us = 1计数）
  */
void TIM4_Init(void)
{
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    
    TIM_InternalClockConfig(TIM4);
    
    TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
    TIM_TimeBaseInitStructure.TIM_ClockDivision = TIM_CKD_DIV1;
    TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInitStructure.TIM_Period = 0xFFFF;      // 最大计数，不自动重载
    TIM_TimeBaseInitStructure.TIM_Prescaler = 72 - 1;   // 72MHz / 72 = 1MHz
    TIM_TimeBaseInitStructure.TIM_RepetitionCounter = 0;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseInitStructure);
    
    TIM_Cmd(TIM4, ENABLE);
}
