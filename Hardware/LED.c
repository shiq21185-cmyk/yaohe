#include "stm32f10x.h"                  // Device header


void LED_Init(void){
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InStructure;
	GPIO_InStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InStructure.GPIO_Pin = GPIO_Pin_0;
	GPIO_InStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InStructure);
	GPIO_SetBits(GPIOA,GPIO_Pin_0);
	
}


void LED0_ON(void)
{
GPIO_ResetBits(GPIOA,GPIO_Pin_0);
}
void LED0_OFF(void)
{
GPIO_SetBits(GPIOA,GPIO_Pin_0);
}
void LED0_Turn(void)
{
    if(GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_0)==0)
	{
		GPIO_SetBits(GPIOA,GPIO_Pin_0);
    }else{
	GPIO_ResetBits(GPIOA,GPIO_Pin_0);
    }
}


