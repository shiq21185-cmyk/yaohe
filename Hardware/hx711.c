#include "stm32f10x.h"
#include "hx711.h"
#include "Delay.h"

u32 HX711_Buffer;
u32 Weight_Maopi;
s32 Weight_Shiwu;
u8 Flag_Error = 0;

/* 标定系数：每克对应的 HX711 原始计数差。 */
#define GapValue 550

/* 初始化 HX711：PB14 输出时钟，PB15 上拉输入数据。 */
void Init_HX711pin(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOB, &GPIO_InitStructure);
    GPIO_SetBits(GPIOB, GPIO_Pin_14);
}

/* 读取 HX711 的 24 位有符号原始数据，并完成符号位转换。 */
u32 HX711_Read(void)
{
    unsigned long count;
    unsigned char i;

    Delay_us(1);
    HX711_SCK_LOW();
    count = 0;
    while(HX711_DOUT_READ() == 1);

    for(i = 0; i < 24; i++)
    {
        HX711_SCK_HIGH();
        count <<= 1;
        Delay_us(1);
        HX711_SCK_LOW();
        if(HX711_DOUT_READ() == 1)
            count++;
        Delay_us(1);
    }
    HX711_SCK_HIGH();
    count ^= 0x800000;
    Delay_us(1);
    HX711_SCK_LOW();
    return count;
}

/* 连续采样五次，取平均值作为空载去皮基准。 */
void Get_Maopi(void)
{
    u32 sum = 0;
    u8 i;

    for(i = 0; i < 5; i++)
    {
        sum += HX711_Read();
        Delay_ms(10);
    }
    Weight_Maopi = sum / 5;
}

/* 采用去极值平均、范围校验和稳定判定计算重量，单位为 0.1 g。 */
void Get_Weight(void)
{
    s32 samples[16];
    s32 sum = 0;
    u8 i;
    s32 diff;
    static s32 last_stable_weight = 0;
    static u8 stable_count = 0;

    for(i = 0; i < 6; i++)
    {
        HX711_Buffer = HX711_Read();
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
    if(diff < -2000 || diff > (s32)(GapValue * 5000))
        return;

    s32 weight_temp = 0;
    if(diff > 0)
        weight_temp = (diff * 10 + GapValue / 2) / GapValue;

    /* 小于 1.0 g 的变化按零处理，降低空载抖动。 */
    if(weight_temp < 10)
        weight_temp = 0;
    if(weight_temp > 50000)
        return;

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
}
