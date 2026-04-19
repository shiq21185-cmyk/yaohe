#ifndef __HX711_H
#define __HX711_H

#include "stm32f10x.h"
#include "stdlib.h"

#define HX711_SCK_HIGH()    GPIO_SetBits(GPIOB, GPIO_Pin_14)    // SCK �ø�
#define HX711_SCK_LOW()     GPIO_ResetBits(GPIOB, GPIO_Pin_14)  // SCK �õ�
#define HX711_DOUT_READ()   GPIO_ReadInputDataBit(GPIOB, GPIO_Pin_15)  // ��ȡDOUT


extern void Init_HX711pin(void);
extern u32 HX711_Read(void);
extern void Get_Maopi(void);
extern void Get_Weight(void);

extern u32 HX711_Buffer;
extern u32 Weight_Maopi;
extern s32 Weight_Shiwu;
extern float Weight_Shiwu_Float;
extern u8 Flag_Error;

#endif

