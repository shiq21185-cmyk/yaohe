#include "stm32f10x.h"                  // Device header
#include "bsp_gpio.h"

/* 板载 LED：PA0，低电平点亮。
 * GPIO 的时钟与配置全部交给 BSP 层，这里只保留语义。 */
#define LED0_PORT   GPIOA
#define LED0_PIN    GPIO_Pin_0

void LED_Init(void)
{
	BSP_GPIO_ConfigOutPP(LED0_PORT, LED0_PIN);
	/* PA0 为低电平点亮，初始化时默认熄灭。 */
	BSP_GPIO_Set(LED0_PORT, LED0_PIN);
}

void LED0_ON(void)
{
	BSP_GPIO_Reset(LED0_PORT, LED0_PIN);
}

void LED0_OFF(void)
{
	BSP_GPIO_Set(LED0_PORT, LED0_PIN);
}

void LED0_Turn(void)
{
	BSP_GPIO_Toggle(LED0_PORT, LED0_PIN);
}
