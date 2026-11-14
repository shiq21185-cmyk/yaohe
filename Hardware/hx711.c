#include "stm32f10x.h"
#include "hx711.h"
#include "Delay.h"
#include "FreeRTOS.h"
#include "task.h"

u32 HX711_Buffer;
u32 Weight_Maopi;
s32 Weight_Shiwu;
u8 Flag_Error = 0;

/* 标定系数：每克对应的 HX711 原始计数差。 */
#define GapValue 550
#define HX711_READY_TIMEOUT_MS 150U

/* 初始化 HX711：PB14 输出时钟，PB15 上拉输入数据。 */
void Init_HX711pin(void)
{
    BSP_GPIO_ConfigOutPP(HX711_SCK_PORT, HX711_SCK_PIN);
    BSP_GPIO_ConfigInPullUp(HX711_DOUT_PORT, HX711_DOUT_PIN);
    HX711_SCK_HIGH();
}

/* 读取 HX711 的 24 位原始数据。等待转换就绪有超时，避免传感器掉线时
 * 永久占住任务；SCK 脉冲期间短暂屏蔽中断，防止被调度打断后高电平过长。 */
uint8_t HX711_Read(u32 *value)
{
    unsigned long count = 0;
    unsigned char i;
    TickType_t start_tick;
    TickType_t next_poll_tick;
    TickType_t timeout_ticks = pdMS_TO_TICKS(HX711_READY_TIMEOUT_MS);

    if (value == 0) { return 0; }
    if (timeout_ticks == 0) { timeout_ticks = 1; }

    HX711_SCK_LOW();
    start_tick = xTaskGetTickCount();
    next_poll_tick = start_tick;
    while (HX711_DOUT_READ() != 0)
    {
        if ((TickType_t)(xTaskGetTickCount() - start_tick) >= timeout_ticks)
        {
            Flag_Error = 1;
            return 0;
        }
        /* Poll at a fixed one-tick cadence while the converter is working. */
        vTaskDelayUntil(&next_poll_tick, 1);
    }

    for (i = 0; i < 24; i++)
    {
        uint32_t irq_state = __get_PRIMASK();
        __disable_irq();
        HX711_SCK_HIGH();
        count <<= 1;
        Delay_us(1);
        HX711_SCK_LOW();
        if (HX711_DOUT_READ() != 0) { count++; }
        Delay_us(1);
        if (irq_state == 0) { __enable_irq(); }
    }

    {
        uint32_t irq_state = __get_PRIMASK();
        __disable_irq();
        HX711_SCK_HIGH();
        Delay_us(1);
        HX711_SCK_LOW();
        if (irq_state == 0) { __enable_irq(); }
    }

    count ^= 0x800000UL;
    *value = (u32)count;
    HX711_Buffer = *value;
    Flag_Error = 0;
    return 1;
}

/* 连续采样五次，任何一次超时都不覆盖上一次有效皮重。 */
uint8_t Get_Maopi(void)
{
    u32 sum = 0;
    u32 sample;
    u8 i;

    for (i = 0; i < 5; i++)
    {
        if (!HX711_Read(&sample)) { return 0; }
        sum += sample;
        Delay_ms(10);
    }
    Weight_Maopi = sum / 5;
    return 1;
}

/* 采用去极值平均、范围校验和稳定判定计算重量，单位为 0.1 g。 */
uint8_t Get_Weight(void)
{
    s32 samples[6];
    s32 sum = 0;
    u8 i;
    s32 diff;
    static s32 last_stable_weight = 0;
    static u8 stable_count = 0;

    for (i = 0; i < 6; i++)
    {
        if (!HX711_Read(&HX711_Buffer)) { return 0; }
        samples[i] = (s32)HX711_Buffer - (s32)Weight_Maopi;
    }

    s32 max = samples[0], min = samples[0];
    for(i = 0; i < 6; i++)
    {
        sum += samples[i];
        if(samples[i] > max) max = samples[i];
        if(samples[i] < min) min = samples[i];
    }
    diff = (sum - max - min) / 4;

    /* 丢弃明显异常的 ADC 读数。 */
    if (diff < -2000 || diff > (s32)(GapValue * 5000))
        return 0;

    s32 weight_temp = 0;
    if(diff > 0)
        weight_temp = (diff * 10 + GapValue / 2) / GapValue;

    /* 小于 1.0 g 的变化按零处理，降低空载抖动。 */
    if(weight_temp < 10)
        weight_temp = 0;
    if (weight_temp > 50000)
        return 0;

    s32 weight_diff = abs(weight_temp - last_stable_weight);
    if(weight_diff > 1)
    {
        Weight_Shiwu = weight_temp;
        last_stable_weight = weight_temp;
        stable_count = 0;
    }
    else if(weight_diff <= 1)
    {
        stable_count++;
        if(stable_count >= 2)
        {
            Weight_Shiwu = weight_temp;
            stable_count = 2;
        }
    }
    else
    {
        stable_count = 0;
        last_stable_weight = weight_temp;
    }

    return 1;
}
