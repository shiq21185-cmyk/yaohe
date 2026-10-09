#include "bsp_i2c.h"
#include "bsp_gpio.h"

/* 当前注册的总线；本工程只有 OLED 一条软件 I2C，未注册时所有操作安全返回 */
static const BSP_SoftI2C_Bus_t *s_bus = 0;

static void i2c_scl(uint8_t level)
{
    if(s_bus != 0)
    {
        BSP_GPIO_Write(s_bus->scl_port, s_bus->scl_pin, level);
    }
}

static void i2c_sda(uint8_t level)
{
    if(s_bus != 0)
    {
        BSP_GPIO_Write(s_bus->sda_port, s_bus->sda_pin, level);
    }
}

void BSP_SoftI2C_Init(const BSP_SoftI2C_Bus_t *bus)
{
    if(bus == 0)
    {
        return;
    }

    s_bus = bus;

    /* 开漏输出：主机只下拉，拉高交给外部上拉电阻 */
    BSP_GPIO_ConfigOutOD(bus->scl_port, bus->scl_pin);
    BSP_GPIO_ConfigOutOD(bus->sda_port, bus->sda_pin);

    /* 空闲态：两根线都释放为高 */
    i2c_scl(1);
    i2c_sda(1);
}

void BSP_SoftI2C_Start(void)
{
    i2c_sda(1);
    i2c_scl(1);
    i2c_sda(0);
    i2c_scl(0);
}

void BSP_SoftI2C_Stop(void)
{
    i2c_sda(0);
    i2c_scl(1);
    i2c_sda(1);
}

void BSP_SoftI2C_WriteByte(uint8_t byte)
{
    uint8_t i;

    for(i = 0; i < 8U; i++)
    {
        i2c_sda((uint8_t)(((byte & (uint8_t)(0x80U >> i)) != 0U) ? 1U : 0U));
        i2c_scl(1);
        i2c_scl(0);
    }

    /* 额外的一个时钟，不处理应答信号（与原实现一致） */
    i2c_scl(1);
    i2c_scl(0);
}

void BSP_SoftI2C_WriteFrame(uint8_t slave_addr, uint8_t control,
                            const uint8_t *data, uint16_t len)
{
    uint16_t i;

    BSP_SoftI2C_Start();
    BSP_SoftI2C_WriteByte(slave_addr);
    BSP_SoftI2C_WriteByte(control);

    for(i = 0; i < len; i++)
    {
        BSP_SoftI2C_WriteByte(data[i]);
    }

    BSP_SoftI2C_Stop();
}
