#include "stm32f10x.h"
#include "hx711.h"
#include "Delay.h"
#include <math.h>
u32 HX711_Buffer;
u32 Weight_Maopi;
s32 Weight_Shiwu;
float Weight_Shiwu_Float;
u8  Flag_Error=0;


#define GapValue 550.5


void Init_HX711pin(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);	

	//HX711_SCK
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_14;				
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; 		
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;		
	GPIO_Init(GPIOB, &GPIO_InitStructure);				
	
	//HX711_DOUT
  GPIO_InitStructure.GPIO_Pin = GPIO_Pin_15;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;     
  GPIO_Init(GPIOB, &GPIO_InitStructure);  
	
  GPIO_SetBits(GPIOB,GPIO_Pin_14);				
}

u32 HX711_Read(void)	
{
	unsigned long count; 
	unsigned char i; 

	Delay_us(1);
  HX711_SCK_LOW(); 
  count=0; 
  while(HX711_DOUT_READ()==1); 
  for(i=0;i<24;i++)
	{ 
	  HX711_SCK_HIGH(); 
	  count=count<<1; 
		Delay_us(1);
		HX711_SCK_LOW(); 
	  if(HX711_DOUT_READ()==1)
		count++; 
		Delay_us(1);
	} 
 	HX711_SCK_HIGH(); 
  count=count^0x800000;
	Delay_us(1);
	HX711_SCK_LOW();  
	return(count);
}

void Get_Maopi(void)
{
    u32 sum = 0;
    u8 i;
    
    // ��ȡ5��ȡƽ�����õ����ȶ���ȥƤֵ
    for(i = 0; i < 5; i++)
    {
        sum += HX711_Read();
        Delay_ms(10);
    }
    Weight_Maopi = sum / 5;
} 


void Get_Weight(void)
{
    s32 samples[16];
    u32 sum = 0;
    u8 i;
    float weight_temp_float;
    s32 diff;
    
    static float last_stable_weight_float = 0.0f;
    static u8 stable_count = 0;
    
    // 采样8次，平衡精度和速度
    for(i = 0; i < 8; i++)
    {
        HX711_Buffer = HX711_Read();
        samples[i] = (s32)HX711_Buffer - (s32)Weight_Maopi;
    }
    
    // 限幅滤波：去掉1个最大值和1个最小值
    s32 max = samples[0], min = samples[0];
    for(i = 0; i < 8; i++)
    {
        sum += samples[i];
        if(samples[i] > max) max = samples[i];
        if(samples[i] < min) min = samples[i];
    }
    // 去掉极值后平均
    sum = sum - max - min;
    diff = sum / 6;
    
    // 异常采样过滤：差值超出合理范围直接丢弃本次采样
    if(diff < -2000 || diff > (s32)(GapValue * 5000))
    {
        return;
    }
    
    // 计算重量（浮点精度）
    if(diff > 0)
    {
        weight_temp_float = (float)diff / GapValue;
    }
    else
    {
        weight_temp_float = 0.0f;
    }
    
    // 小重量阈值
    if(weight_temp_float < 0.5f)
    {
        weight_temp_float = 0.0f;
    }
    
    // 异常值二次过滤：重量超过5kg直接丢弃
    if(weight_temp_float > 5000.0f)
    {
        return;
    }
    
    // 稳定防抖逻辑
    float weight_diff_float = fabsf(weight_temp_float - last_stable_weight_float);
    
    // 大重量变化（>1.0g）直接响应
    if(weight_diff_float > 1.0f)
    {
        Weight_Shiwu = (s32)weight_temp_float;
        Weight_Shiwu_Float = weight_temp_float;
        last_stable_weight_float = weight_temp_float;
        stable_count = 0;
    }
    // 中小变化：连续2次偏差小于0.5g才更新
    else if(weight_diff_float <= 0.5f)
    {
        stable_count++;
        if(stable_count >= 2)
        {
            Weight_Shiwu = (s32)weight_temp_float;
            Weight_Shiwu_Float = weight_temp_float;
            stable_count = 2;
        }
    }
    else
    {
        stable_count = 0;
        last_stable_weight_float = weight_temp_float;
    }
}
