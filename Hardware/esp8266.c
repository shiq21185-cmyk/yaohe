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
#include "FreeRTOS.h"
#include "task.h"

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


/* 本工程只使用短 AT/MQTT 状态响应，不订阅大载荷；保留 256B 足够的接收空间。 */
#define ESP8266_RX_BUF_SIZE 256U
unsigned char esp8266_buf[ESP8266_RX_BUF_SIZE];
volatile unsigned short esp8266_cnt = 0, esp8266_cntPre = 0;

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
    uint32_t primask = __get_PRIMASK();
    __disable_irq();
    esp8266_cnt = 0;
    esp8266_cntPre = 0;
    esp8266_buf[0] = '\0';
    __set_PRIMASK(primask);
}

_Bool ESP8266_WaitRecive(void)
{
    uint32_t primask = __get_PRIMASK();
    unsigned short count;
    _Bool result = REV_WAIT;

    __disable_irq();
    count = esp8266_cnt;
    if (count != 0)
    {
        if (count == esp8266_cntPre)
        {
            esp8266_cnt = 0;
            esp8266_cntPre = 0;
            result = REV_OK;
        }
        else
        {
            esp8266_cntPre = count;
        }
    }
    __set_PRIMASK(primask);
    return result;
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

    sprintf(cmd, "AT+MQTTCONN=0,\"%s\",%d,1\r\n", MQTT_BROKER_IP, MQTT_BROKER_PORT);
    ESP8266_Clear();

	while(ESP8266_SendCmd(cmd, "MQTTCONNECTED"));

}

// 发布 MQTT 消息。成功返回 1；失败/超时返回 0，由上层保留队列并稍后重试。
unsigned char ESP_MQTTPublish(unsigned char *topic, unsigned char *data)
{
    char *cmd = esp_cmd_buf;
    uint16_t waited_ms = 0;
    uint8_t scheduler_running;
    uint8_t response;
    uint32_t primask;
    size_t topic_len;
    size_t data_len;

    if (topic == 0 || data == 0) return 0;
    topic_len = strlen((const char *)topic);
    data_len = strlen((const char *)data);
    if (topic_len + data_len + 64U >= ESP_CMD_BUF_SIZE) return 0;

    sprintf(cmd, "AT+MQTTPUB=0,\"%s\",\"%s\",1,0\r\n",
            (const char *)topic, (const char *)data);
    ESP8266_Clear();
    BSP_UART_Send(BSP_UART_ESP, (const uint8_t *)cmd, (uint16_t)strlen(cmd));
    scheduler_running = (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) ? 1U : 0U;

    while (waited_ms < 10000U)
    {
        /* RX ISR 追加数据时短暂屏蔽中断，避免扫描到半写入的字符串。 */
        primask = __get_PRIMASK();
        __disable_irq();
        response = 0;
        if (strstr((const char *)esp8266_buf, "ERROR") != 0 ||
            strstr((const char *)esp8266_buf, "FAIL") != 0 ||
            strstr((const char *)esp8266_buf, "ERR CODE:") != 0)
        {
            response = 2;
        }
        else if (strstr((const char *)esp8266_buf, "\r\nOK\r\n") != 0 ||
                 strncmp((const char *)esp8266_buf, "OK\r\n", 4U) == 0)
        {
            /* AT+MQTTPUB 字符串模式以 OK 表示本次发布命令成功。 */
            response = 1;
        }
        __set_PRIMASK(primask);

        if (response == 1)
        {
            ESP8266_Clear();
            return 1;
        }
        if (response == 2)
        {
            ESP8266_Clear();
            return 0;
        }

        if (scheduler_running)
            vTaskDelay(pdMS_TO_TICKS(10));
        else
            Delay_ms(10);
        waited_ms += 10U;
    }

    ESP8266_Clear();
    return 0;
}

void USART2_IRQHandler(void)
{
    int b = BSP_UART_IsrFetch(BSP_UART_ESP);
    if (b >= 0 && esp8266_cnt < (sizeof(esp8266_buf) - 1U))
    {
        esp8266_buf[esp8266_cnt++] = (unsigned char)b;
        esp8266_buf[esp8266_cnt] = '\0';
    }
}
