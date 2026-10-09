#include "stm32f10x.h"                  // Device header
#include "bsp_gpio.h"

/* 光敏模块数字输出 DO：PB4，上拉输入。
 * 模块上的电位器决定"见光为高"还是"遮光为高"，本层不做极性假设。 */
#define LIGHT_PORT  GPIOB
#define LIGHT_PIN   GPIO_Pin_4

void LightSenor_Init(void)
{
	BSP_GPIO_ConfigInPullUp(LIGHT_PORT, LIGHT_PIN);
}

uint8_t LightSenor_Get(void)
{
	return BSP_GPIO_Read(LIGHT_PORT, LIGHT_PIN);
}
