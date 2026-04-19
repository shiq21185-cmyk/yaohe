#ifndef __OLED_H
#define __OLED_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "semphr.h"

// OLED互斥锁句柄（外部声明）
extern SemaphoreHandle_t xOLEDMutex;

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Line, uint8_t Column, char Char);
void OLED_ShowString(uint8_t Line, uint8_t Column, char *String);
void OLED_ShowNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowSignedNum(uint8_t Line, uint8_t Column, int32_t Number, uint8_t Length);
void OLED_ShowHexNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
void OLED_ShowBinNum(uint8_t Line, uint8_t Column, uint32_t Number, uint8_t Length);
//Chinese
void OLED_ShowChinese(uint8_t Line, uint8_t Column, uint8_t Index);
//长度变长
void OLED_ShowBigNum(uint8_t Line, uint8_t Column, uint8_t Number);
void OLED_ShowBigChar(uint8_t Line, uint8_t Column, char Char);
//高度变宽
void OLED_ShowTallNum(uint8_t Line, uint8_t Column, uint8_t Number);
void OLED_ShowTallChar(uint8_t Line, uint8_t Column, char Char);


void OLED_Show16x32Char(uint8_t Line, uint8_t Column, char Char);
#endif
