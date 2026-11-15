#include "stm32f10x.h"
#include "dht11.h"
#include "Delay.h"
#include "bsp_time.h"

/* 最近一次通过校验的数据；通信失败时保持不变，调用方必须检查返回值。 */
unsigned int rec_data[4] = {0};

void DH11_GPIO_Init_OUT(void)
{
    BSP_GPIO_ConfigOutPP(DHT11_PORT, DHT11_PIN);
}

void DH11_GPIO_Init_IN(void)
{
    BSP_GPIO_ConfigInFloat(DHT11_PORT, DHT11_PIN);
}

void DHT11_Start(void)
{
    DH11_GPIO_Init_OUT();
    dht11_high;
    Delay_us(30);
    dht11_low;
    Delay_ms(20);
    dht11_high;
    Delay_us(30);
    DH11_GPIO_Init_IN();
}

/* TIM4 为 1 MHz 自由运行计数器；无符号差值可正确跨越 16 位回绕。 */
static uint8_t DHT11_WaitLevel(uint8_t level, uint16_t timeout_us)
{
    uint16_t start = BSP_Time_GetCounter();

    while ((uint8_t)Read_Data != level)
    {
        if ((uint16_t)(BSP_Time_GetCounter() - start) >= timeout_us)
            return 0;
    }
    return 1;
}

static uint8_t DHT11_ReadByte(uint8_t *value)
{
    uint8_t i;
    uint8_t data = 0;

    for (i = 0; i < 8; i++)
    {
        if (!DHT11_WaitLevel(1, 100)) return 0;
        Delay_us(40);
        data <<= 1;
        if ((uint8_t)Read_Data != 0) data |= 1U;
        if (!DHT11_WaitLevel(0, 100)) return 0;
    }
    *value = data;
    return 1;
}

/* 成功返回 1；超时或校验失败返回 0，绝不把旧帧报告为新帧。 */
uint8_t DHT11_REC_Data(void)
{
    uint8_t frame[5];
    uint8_t i;
    uint32_t primask;

    DHT11_Start();
    if (!DHT11_WaitLevel(0, 120) ||
        !DHT11_WaitLevel(1, 120) ||
        !DHT11_WaitLevel(0, 120))
        goto read_failed;

    /* 仅在微秒级 40 位窗口关闭中断，之后恢复原有 PRIMASK 状态。 */
    primask = __get_PRIMASK();
    __disable_irq();
    for (i = 0; i < 5; i++)
    {
        if (!DHT11_ReadByte(&frame[i]))
        {
            __set_PRIMASK(primask);
            goto read_failed;
        }
    }
    __set_PRIMASK(primask);

    dht11_low;
    Delay_us(55);
    dht11_high;

    /* DHT11 校验和是 8 位累加，必须按 uint8_t 回绕比较。 */
    if ((uint8_t)(frame[0] + frame[1] + frame[2] + frame[3]) != frame[4])
    {
        DH11_GPIO_Init_IN();
        return 0;
    }

    rec_data[0] = frame[0];
    rec_data[1] = frame[1];
    rec_data[2] = frame[2];
    rec_data[3] = frame[3];
    DH11_GPIO_Init_IN();
    return 1;

read_failed:
    dht11_high;
    DH11_GPIO_Init_IN();
    return 0;
}
