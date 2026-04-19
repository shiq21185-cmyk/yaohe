#ifndef __DHT11_H
#define __DHT11_H
#include "stm32f10x.h"

#define dht11_high GPIO_SetBits(GPIOA, GPIO_Pin_5)
#define dht11_low GPIO_ResetBits(GPIOA, GPIO_Pin_5)
#define Read_Data GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_5)

// 添加外部变量声明
extern unsigned int rec_data[4];

void DH11_GPIO_Init_OUT(void);
void DH11_GPIO_Init_IN(void);
void DHT11_Start(void);
char DHT11_Rec_Byte(void);  // 修正返回类型
void DHT11_REC_Data(void);  // 修正函数名

#endif
