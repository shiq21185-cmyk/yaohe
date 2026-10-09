#include "bsp_gpio.h"

/* ==========================================================================
 * GPIO 语义层实现
 * ========================================================================== */

void BSP_GPIO_EnableClock(GPIO_TypeDef *port)
{
    uint32_t periph;

    if      (port == GPIOA) { periph = RCC_APB2Periph_GPIOA; }
    else if (port == GPIOB) { periph = RCC_APB2Periph_GPIOB; }
    else if (port == GPIOC) { periph = RCC_APB2Periph_GPIOC; }
    else if (port == GPIOD) { periph = RCC_APB2Periph_GPIOD; }
    else if (port == GPIOE) { periph = RCC_APB2Periph_GPIOE; }
    else { return; }   /* 未知端口不当成 GPIOA 处理，避免误开时钟 */

    RCC_APB2PeriphClockCmd(periph, ENABLE);
}

void BSP_GPIO_Config(GPIO_TypeDef *port, uint16_t pin,
                     GPIOMode_TypeDef mode, GPIOSpeed_TypeDef speed)
{
    GPIO_InitTypeDef init;

    if (port == 0) { return; }

    BSP_GPIO_EnableClock(port);

    init.GPIO_Pin   = pin;
    init.GPIO_Speed = speed;
    init.GPIO_Mode  = mode;
    GPIO_Init(port, &init);
}

void BSP_GPIO_ConfigOutPP(GPIO_TypeDef *port, uint16_t pin)
{
    BSP_GPIO_Config(port, pin, GPIO_Mode_Out_PP, GPIO_Speed_50MHz);
}

void BSP_GPIO_ConfigOutOD(GPIO_TypeDef *port, uint16_t pin)
{
    BSP_GPIO_Config(port, pin, GPIO_Mode_Out_OD, GPIO_Speed_50MHz);
}

void BSP_GPIO_ConfigAFPP(GPIO_TypeDef *port, uint16_t pin)
{
    BSP_GPIO_Config(port, pin, GPIO_Mode_AF_PP, GPIO_Speed_50MHz);
}

void BSP_GPIO_ConfigInFloat(GPIO_TypeDef *port, uint16_t pin)
{
    BSP_GPIO_Config(port, pin, GPIO_Mode_IN_FLOATING, GPIO_Speed_50MHz);
}

void BSP_GPIO_ConfigInPullUp(GPIO_TypeDef *port, uint16_t pin)
{
    BSP_GPIO_Config(port, pin, GPIO_Mode_IPU, GPIO_Speed_50MHz);
}

void BSP_GPIO_ConfigInPullDown(GPIO_TypeDef *port, uint16_t pin)
{
    BSP_GPIO_Config(port, pin, GPIO_Mode_IPD, GPIO_Speed_50MHz);
}

void BSP_GPIO_ConfigAnalog(GPIO_TypeDef *port, uint16_t pin)
{
    BSP_GPIO_Config(port, pin, GPIO_Mode_AIN, GPIO_Speed_50MHz);
}

void BSP_GPIO_Write(GPIO_TypeDef *port, uint16_t pin, uint8_t level)
{
    GPIO_WriteBit(port, pin, (level != 0u) ? Bit_SET : Bit_RESET);
}

void BSP_GPIO_Set(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_SetBits(port, pin);
}

void BSP_GPIO_Reset(GPIO_TypeDef *port, uint16_t pin)
{
    GPIO_ResetBits(port, pin);
}

void BSP_GPIO_Toggle(GPIO_TypeDef *port, uint16_t pin)
{
    if (GPIO_ReadOutputDataBit(port, pin) == Bit_SET)
    {
        GPIO_ResetBits(port, pin);
    }
    else
    {
        GPIO_SetBits(port, pin);
    }
}

uint8_t BSP_GPIO_Read(GPIO_TypeDef *port, uint16_t pin)
{
    return (GPIO_ReadInputDataBit(port, pin) == Bit_SET) ? 1u : 0u;
}

uint8_t BSP_GPIO_ReadOutput(GPIO_TypeDef *port, uint16_t pin)
{
    return (GPIO_ReadOutputDataBit(port, pin) == Bit_SET) ? 1u : 0u;
}
