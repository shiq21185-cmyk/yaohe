#ifndef __HX711_H
#define __HX711_H

#include "stm32f10x.h"
#include "stdlib.h"
#include "bsp_gpio.h"

/* HX711 数据接口：SCK 使用 PB14，DOUT 使用 PB15。
 * 时序宏改走 BSP_GPIO，驱动层不再直接触碰 GPIO 寄存器。 */
#define HX711_SCK_PORT    GPIOB
#define HX711_SCK_PIN     GPIO_Pin_14
#define HX711_DOUT_PORT   GPIOB
#define HX711_DOUT_PIN    GPIO_Pin_15

#define HX711_SCK_HIGH()  BSP_GPIO_Set(HX711_SCK_PORT, HX711_SCK_PIN)
#define HX711_SCK_LOW()   BSP_GPIO_Reset(HX711_SCK_PORT, HX711_SCK_PIN)
/* 返回 0/1；HX711_Read 里与 == 1 比较，语义不变。 */
#define HX711_DOUT_READ() BSP_GPIO_Read(HX711_DOUT_PORT, HX711_DOUT_PIN)

void Init_HX711pin(void);
u32 HX711_Read(void);
void Get_Maopi(void);
void Get_Weight(void);

extern u32 HX711_Buffer;
extern u32 Weight_Maopi;
extern s32 Weight_Shiwu;
extern u8 Flag_Error;

#endif
