//芯片头文件
#include "stm32f10x.h"

//功能模块头文件
#include "esp8266.h"

//硬件驱动
#include "delay.h"
#include "usart.h"
#include "OLED.h"

//板级支持包
#include "bsp_gpio.h"
#include "bsp_uart.h"

//C库
#include <string.h>
#include <stdio.h>

//#define ESP8266_WIFI_INFO		"AT+CWJAP=\"666\",\"88888888\"\r\n"
#define ESP8266_WIFI_INFO		"AT+CWJAP=\"xwb\",\"xwb123456\"\r\n"
#define MQTT_BROKER_IP			"broker.hivemq.com"
#define MQTT_BROKER_PORT	    1883
#define MQTT_CLIENT_ID 			"1242451541"					//客户端id
#define MQTT_USER ""
#define MQTT_PWD ""


unsigned char esp8266_buf[1280];
unsigned short esp8266_cnt = 0, esp8266_cntPre = 0;

/* AT 命令组装的公用缓冲。
 * 原先 ESP_ConnectMQTT() 和 ESP_MQTTPublish() 各自在栈上开 char cmd[256]，
 * 把这两个函数的栈帧顶到 264 / 280 字节，进而逼着 main 的主栈(MSP)和
 * Upload_Task 的任务栈都按这个最坏情况预留空间。
 * 两者不会并发——ESP_ConnectMQTT() 只在 main() 里、调度器启动之前调用，
 * ESP_MQTTPublish() 只在 Upload_Task 里调用——所以合并成一个静态缓冲。
 * 代价是 .bss 多 256 字节，换来主栈和 Upload_Task 栈各可少留 512 字节。 */
#define ESP_CMD_BUF_SIZE 256
static char esp_cmd_buf[ESP_CMD_BUF_SIZE];



void ESP8266_Clear(void)
{

	memset(esp8266_buf, 0, sizeof(esp8266_buf));
	esp8266_cnt = 0;

}


_Bool ESP8266_WaitRecive(void)
{

	if(esp8266_cnt == 0) 							//如果接收计数为0，说明没有收到数据，直接返回等待
		return REV_WAIT;

	if(esp8266_cnt == esp8266_cntPre)				//如果与上次值相同，说明接收完成
	{
		esp8266_cnt = 0;							//清空接收计数

		return REV_OK;								//返回接收完成标志
	}

	esp8266_cntPre = esp8266_cnt;					//设为相同

	return REV_WAIT;								//返回接收未完成标志

}


_Bool ESP8266_SendCmd(char *cmd, char *res) //cmd为命令，res为返回关键字
{

	unsigned char timeOut = 200;

	BSP_UART_Send(BSP_UART_ESP, (const uint8_t *)cmd, (uint16_t)strlen((const char *)cmd));

	while(timeOut--)
	{
		if(ESP8266_WaitRecive() == REV_OK)							//等待接收完成
		{
			if(strstr((const char *)esp8266_buf, res) != NULL)		//检测是否包含关键字
		{
			ESP8266_Clear();									//清空缓存

			return 0;
		}
		}
		Delay_ms(10);
	}

	return 1;

}


void ESP8266_SendData(unsigned char *data, unsigned short len)
{

	char cmdBuf[200];

	ESP8266_Clear();								//清空发送缓存
	sprintf(cmdBuf, "AT+CIPSEND=%d\r\n", len);		//发送指令
	if(!ESP8266_SendCmd(cmdBuf, ">"))			//收到>时可以发送数据
	{
		BSP_UART_Send(BSP_UART_ESP, data, len);		//向设备发送数据
	}

}


/*unsigned char *ESP8266_GetIPD(unsigned short timeOut)
{

	char *ptrIPD = NULL;

	do
	{
		if(ESP8266_WaitRecive() == REV_OK)								//等待接收完成
		{
			ptrIPD = strstr((char *)esp8266_buf, "IPD,");				//检测到IPD开头
			if(ptrIPD != NULL)
			{
				ptrIPD = strchr(ptrIPD, ':');							//找到':'
				if(ptrIPD != NULL)
				{
					ptrIPD++;
					return (unsigned char *)(ptrIPD);
				}
				else
					return NULL;

			}
		}
		delay_ms(5);													//延时等待
	} while(timeOut--);

	return NULL;														//超时未找到数据返回空指针

}*/


void ESP8266_Init(void)
{

	/* ESP8266 复位引脚 PA1，低电平复位（原注释写作 GPIOA0 是笔误）。
	 * 时钟与配置交给 BSP_GPIO。 */
	BSP_GPIO_ConfigOutPP(GPIOA, GPIO_Pin_1);

	BSP_GPIO_Reset(GPIOA, GPIO_Pin_1);
	Delay_ms(250);
	BSP_GPIO_Set(GPIOA, GPIO_Pin_1);
	Delay_ms(500);

	ESP8266_Clear();


	while(ESP8266_SendCmd("AT\r\n", "OK"))
		Delay_ms(500);

	while(ESP8266_SendCmd("AT+CWMODE=1\r\n", "OK"))
		Delay_ms(500);

	while(ESP8266_SendCmd(ESP8266_WIFI_INFO, "GOT IP"))
		Delay_ms(500);


}



// 连接MQTT服务器
void ESP_ConnectMQTT(void)
{
    // 配置MQTT客户端信息（缓冲改用文件级静态数组，不再占用本函数栈帧）
    char *cmd = esp_cmd_buf;
    sprintf(cmd, "AT+MQTTUSERCFG=0,1,\"%s\",\"%s\",\"%s\",0,0,\"\"\r\n",MQTT_CLIENT_ID, MQTT_USER, MQTT_PWD);
    ESP8266_Clear();

    while(ESP8266_SendCmd(cmd, "OK"));
    // 连接MQTT服务器

    sprintf(cmd, "AT+MQTTCONN=0,\"%s\",%d,0\r\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);
    ESP8266_Clear();

	while(ESP8266_SendCmd(cmd, "MQTTCONNECTED"));

}

// 发布MQTT消息
void ESP_MQTTPublish(uint8_t *topic, uint8_t *data)
{
    char *cmd = esp_cmd_buf;
    uint8_t retry_count = 0;

    // 构建发布命令
    sprintf(cmd, "AT+MQTTPUB=0,\"%s\",\"%s\",1,0\r\n", topic, data);

    // 重试最多2次
    while(retry_count < 2)
    {
        ESP8266_Clear();

        // 发送命令
        BSP_UART_Send(BSP_UART_ESP, (const uint8_t *)cmd, (uint16_t)strlen(cmd));

        // 等待响应，超时处理
        unsigned char timeOut = 50;  // 5秒超时
        while(timeOut--)
        {
            Delay_ms(100);

            if(ESP8266_WaitRecive() == REV_OK)
            {
                // 检测响应
                if(strstr((const char *)esp8266_buf, "OK") != NULL)
                {
                    ESP8266_Clear();
                    return;  // 成功
                }
                else if(strstr((const char *)esp8266_buf, "ERROR") != NULL)
                {
                    break;  // 发送失败
                }
            }
        }

        retry_count++;
        Delay_ms(500);
    }

    ESP8266_Clear();  // 清空缓存
}


void USART2_IRQHandler(void)
{
	int b = BSP_UART_IsrFetch(BSP_UART_ESP);   // 判标志位 + 读 DR + 清标志都在 BSP 里

	if(b >= 0)
	{
		if(esp8266_cnt >= sizeof(esp8266_buf))	esp8266_cnt = 0; //防止缓存溢出
		esp8266_buf[esp8266_cnt++] = (unsigned char)b;
	}
}
