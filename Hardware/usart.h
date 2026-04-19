#ifndef __USART_H
#define __USART_H

#include "stm32f10x.h"

void USART2_Init(uint32_t baudrate);
void USART1_Init(uint32_t baudrate); 
void Usart_SendString(USART_TypeDef* USARTx, uint8_t *Data, uint16_t Len);

#endif
