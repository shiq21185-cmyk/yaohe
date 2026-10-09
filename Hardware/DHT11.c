#include "stm32f10x.h"
#include "dht11.h"
#include "Delay.h"

/* DHT11 最近一次校验成功的湿度和温度数据。 */
unsigned int rec_data[4] = {0};

/* 将 PA5 配置为推挽输出，用于发送 DHT11 起始信号。 */
void DH11_GPIO_Init_OUT(void)
{
    BSP_GPIO_ConfigOutPP(DHT11_PORT, DHT11_PIN);
}

/* 将 PA5 配置为浮空输入，用于读取 DHT11 返回数据。 */
void DH11_GPIO_Init_IN(void)
{
    BSP_GPIO_ConfigInFloat(DHT11_PORT, DHT11_PIN);
}

/* 发送低电平起始信号，并切换为输入等待传感器响应。 */
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

/* 按 DHT11 时序读取一个字节：40 us 后仍为高电平表示逻辑 1。 */
char DHT11_Rec_Byte(void)
{
    unsigned char i = 0;
    unsigned char data = 0;

    for(i = 0; i < 8; i++)
    {
        while(Read_Data == 0);
        Delay_us(40);
        data <<= 1;
        if(Read_Data == 1)
        {
            data |= 1;
        }
        while(Read_Data == 1);
    }
    return data;
}

/* 读取 DHT11 的 5 字节帧，并在校验和正确时更新测量结果。
 *
 * 关中断范围说明：原实现在调用方用 __disable_irq() 把整个本函数罩住，
 * 连 20ms 起始脉冲也一起关中断，导致 FreeRTOS tick 与三个串口的接收
 * 中断一次被推迟 20ms 以上（每 500ms 一次，丢事件是必然的）。
 * 现在拆成两段：
 *   - 20ms 起始脉冲 + 传感器应答沿检测：中断保持开启（这 20ms 是纯软件
 *     延时，期间没有任何需要微秒级精度的采样）；
 *   - 只有 40 位数据的采样窗口（5 字节，约 3~5ms）关中断。
 * DHT11 的采样时序仍受保护，而 tick/串口最多被推迟一个采样窗口。 */
void DHT11_REC_Data(void)
{
    unsigned char R_H, R_L, T_H, T_L;
    unsigned char CHECK;

    DHT11_Start();
    if(Read_Data == 0)
    {
        while(Read_Data == 0);
        while(Read_Data == 1);

        __disable_irq();
        R_H = DHT11_Rec_Byte();
        R_L = DHT11_Rec_Byte();
        T_H = DHT11_Rec_Byte();
        T_L = DHT11_Rec_Byte();
        CHECK = DHT11_Rec_Byte();
        __enable_irq();

        dht11_low;
        Delay_us(55);
        dht11_high;

        if((R_H + R_L + T_H + T_L) == CHECK)
        {
            rec_data[0] = R_H;
            rec_data[1] = R_L;
            rec_data[2] = T_H;
            rec_data[3] = T_L;
        }
    }
}
