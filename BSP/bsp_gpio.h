#ifndef __BSP_GPIO_H
#define __BSP_GPIO_H

#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include <stdint.h>

/* ==========================================================================
 * GPIO 语义层
 * --------------------------------------------------------------------------
 * 把「使能端口时钟 -> 填 GPIO_InitTypeDef -> GPIO_Init」这三步从各个驱动
 * 里收回来。驱动只声明"这个引脚要干什么"，不再直接碰 RCC 与 GPIO 寄存器。
 *
 * 约定：
 *   1) 所有函数内部都会先使能该端口的 APB2 时钟，重复调用是幂等的
 *      （RCC 使能位置 1 后再写 1 不产生任何影响）。
 *   2) Config* 系列每次都会重写该引脚在 CRL/CRH 里的 4 个配置位，
 *      不要求调用者先复位引脚。
 *   3) 参数一律用 SPL 的 uint16_t pin（GPIO_Pin_x），便于与旧代码对照。
 * ========================================================================== */

/* 使能 GPIOA..GPIOE 中某个端口的 APB2 时钟；传其它指针时安全返回 */
void BSP_GPIO_EnableClock(GPIO_TypeDef *port);

/* 通用入口：自己指定模式与速度（需要精细控制时用） */
void BSP_GPIO_Config(GPIO_TypeDef *port, uint16_t pin,
                     GPIOMode_TypeDef mode, GPIOSpeed_TypeDef speed);

/* 推挽输出：LED、单总线主机拉低、片选、舵机以外的普通输出 */
void BSP_GPIO_ConfigOutPP(GPIO_TypeDef *port, uint16_t pin);

/* 开漏输出：软件 I2C 的 SCL/SDA（依赖外部上拉，可线与） */
void BSP_GPIO_ConfigOutOD(GPIO_TypeDef *port, uint16_t pin);

/* 复用推挽输出：USART TX、SPI SCK/MOSI、定时器 PWM 输出脚 */
void BSP_GPIO_ConfigAFPP(GPIO_TypeDef *port, uint16_t pin);

/* 浮空输入：USART RX、以及对端已有上拉的单总线 */
void BSP_GPIO_ConfigInFloat(GPIO_TypeDef *port, uint16_t pin);

/* 上拉输入：按键、DHT11 数据线（空闲需为高） */
void BSP_GPIO_ConfigInPullUp(GPIO_TypeDef *port, uint16_t pin);

/* 下拉输入 */
void BSP_GPIO_ConfigInPullDown(GPIO_TypeDef *port, uint16_t pin);

/* 模拟输入：ADC 通道 */
void BSP_GPIO_ConfigAnalog(GPIO_TypeDef *port, uint16_t pin);

/* 电平操作（level 非 0 即高） */
void BSP_GPIO_Write(GPIO_TypeDef *port, uint16_t pin, uint8_t level);
void BSP_GPIO_Set(GPIO_TypeDef *port, uint16_t pin);
void BSP_GPIO_Reset(GPIO_TypeDef *port, uint16_t pin);
void BSP_GPIO_Toggle(GPIO_TypeDef *port, uint16_t pin);

/* 读引脚实际电平（IDR）/ 读输出锁存（ODR），返回 0 或 1 */
uint8_t BSP_GPIO_Read(GPIO_TypeDef *port, uint16_t pin);
uint8_t BSP_GPIO_ReadOutput(GPIO_TypeDef *port, uint16_t pin);

#endif
