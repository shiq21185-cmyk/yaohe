#include "stm32f10x.h"
#include "hx711.h"
#include "Delay.h"

u32 HX711_Buffer;
u32 Weight_Maopi;
float Weight_Shiwu;
u8 Flag_Error = 0;

/*
 * Weight_Maopi 对应教程里的 reset（空载值）
 * HX711_CAL_RAW_100G 对应教程里的 Weights_100（100g时读数）
 * 重量公式：weight = (value - reset) * 100 / (Weights_100 - reset)
 */
 
#define HX711_CAL_WEIGHT_G       100.0f
#define HX711_CAL_RAW_100G       8493860UL

#define HX711_SAMPLE_COUNT       8
#define HX711_ZERO_DEADBAND_G    0.2f
#define HX711_MAX_WEIGHT_G       5000.0f

static u32 HX711_ReadFilteredRaw(void);

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

u32 HX711_Read(void)
{
	unsigned long count;
	unsigned char i;

	Delay_us(1);
	HX711_SCK_LOW();
	count = 0;

	while (HX711_DOUT_READ() == 1);

	for (i = 0; i < 24; i++)
	{
		HX711_SCK_HIGH();
		count = count << 1;
		Delay_us(1);
		HX711_SCK_LOW();
		if (HX711_DOUT_READ() == 1)
		{
			count++;
		}
		Delay_us(1);
	}

	HX711_SCK_HIGH();
	count = count ^ 0x800000;
	Delay_us(1);
	HX711_SCK_LOW();

	return count;
}

void Get_Maopi(void)
{
	u32 sum = 0;
	u8 i;

	for (i = 0; i < HX711_SAMPLE_COUNT; i++)
	{
		sum += HX711_Read();
		Delay_ms(10);
	}

	Weight_Maopi = sum / HX711_SAMPLE_COUNT;
}

static u32 HX711_ReadFilteredRaw(void)
{
	u32 samples[HX711_SAMPLE_COUNT];
	u32 sum = 0;
	u32 max;
	u32 min;
	u8 i;

	for (i = 0; i < HX711_SAMPLE_COUNT; i++)
	{
		HX711_Buffer = HX711_Read();
		samples[i] = HX711_Buffer;
		Delay_ms(2);
	}

	max = samples[0];
	min = samples[0];

	for (i = 0; i < HX711_SAMPLE_COUNT; i++)
	{
		sum += samples[i];
		if (samples[i] > max)
		{
			max = samples[i];
		}
		if (samples[i] < min)
		{
			min = samples[i];
		}
	}

	/* 去掉一个最大值和一个最小值，减少瞬时跳动对精度的影响 */
	sum = sum - max - min;
	return sum / (HX711_SAMPLE_COUNT - 2);
}

void Get_Weight(void)
{
	u32 raw_value;
	u32 cal_diff_count;
	u32 diff_count;
	float weight_temp;

	raw_value = HX711_ReadFilteredRaw();

	if (raw_value <= Weight_Maopi)
	{
		Weight_Shiwu = 0.0f;
		return;
	}

	if (HX711_CAL_RAW_100G <= Weight_Maopi)
	{
		Flag_Error = 1;
		Weight_Shiwu = 0.0f;
		return;
	}

	Flag_Error = 0;
	diff_count = raw_value - Weight_Maopi;
	cal_diff_count = HX711_CAL_RAW_100G - Weight_Maopi;
	weight_temp = ((float)diff_count * HX711_CAL_WEIGHT_G) / (float)cal_diff_count;

	if (weight_temp < HX711_ZERO_DEADBAND_G)
	{
		weight_temp = 0.0f;
	}

	if (weight_temp > HX711_MAX_WEIGHT_G)
	{
		Weight_Shiwu = HX711_MAX_WEIGHT_G;
		return;
	}

	Weight_Shiwu = weight_temp;
}
