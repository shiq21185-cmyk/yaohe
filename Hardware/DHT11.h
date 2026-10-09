#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f10x.h"
#include "bsp_gpio.h"

/* DHT11 数据线接在 PA5。单总线要双向，所以读写都经 BSP_GPIO 语义层，
 * 头文件里不再出现 GPIO_SetBits / GPIO_ReadInputDataBit 这类寄存器调用。 */
#define DHT11_PORT      GPIOA
#define DHT11_PIN       GPIO_Pin_5

#define dht11_high      BSP_GPIO_Set(DHT11_PORT, DHT11_PIN)
#define dht11_low       BSP_GPIO_Reset(DHT11_PORT, DHT11_PIN)
/* 返回 0/1，原代码里与 == 1 / == 0 比较，语义不变。 */
#define Read_Data       BSP_GPIO_Read(DHT11_PORT, DHT11_PIN)

/* 湿度整数/小数与温度整数/小数，依次存放在 0 至 3 下标。 */
extern unsigned int rec_data[4];

void DH11_GPIO_Init_OUT(void);
void DH11_GPIO_Init_IN(void);
void DHT11_Start(void);
char DHT11_Rec_Byte(void);
void DHT11_REC_Data(void);

#endif
