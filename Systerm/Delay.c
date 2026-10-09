#include "Delay.h"
#include "bsp_time.h"

/* ---------------------------------------------------------------------------
 * 三个延时函数现在只是 BSP 时基层的兼容包装：
 * TIM4 的 1 MHz 时基配置与寄存器操作全部收敛在 BSP/bsp_time.c 里。
 * 原来的 Delay_us1 / Delay_ms1 / Delay_s1（空函数体、无任何调用者）已删除。
 * --------------------------------------------------------------------------- */

void Delay_us(uint32_t us)
{
    BSP_DelayUs(us);
}

void Delay_ms(uint32_t ms)
{
    BSP_DelayMs(ms);
}

void Delay_s(uint32_t s)
{
    BSP_DelayS(s);
}
