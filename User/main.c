#include "stm32f10x.h"
#include "Delay.h"
#include "OLED.h"
#include "Timer.h"
#include "LED.h"
#include "sys.h"
#include "esp8266.h"
#include "dht11.h"
#include "hx711.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "xrvoice.h"
#include "hc06.h"
#include "key.h"
#include "app_tasks.h"
#include "Servo.h"
#include "PWM.h"

// 外部时间互斥量
extern SemaphoreHandle_t xTimeMutex;

int main(void)
{
    BaseType_t xReturn = pdPASS;
    
    LED_Init();
    OLED_Init();
    
    // OLED互斥量
    extern SemaphoreHandle_t xOLEDMutex;
    xOLEDMutex = xSemaphoreCreateMutex();
    if(xOLEDMutex == NULL)
    {
        OLED_ShowString(1, 1, "Mutex Fail");
        while(1);
    }
    
    // 时间互斥量
    xTimeMutex = xSemaphoreCreateMutex();
    if(xTimeMutex == NULL)
    {
        OLED_ShowString(1, 1, "TimeMutex Fail");
        while(1);
    }
    
    TIM4_Init();
    
    HC06_Init();
    USART1_Init(9600);      // USART1初始化
    USART2_Init(115200);    // USART2连接ESP8266
    
    Key_Init();
    
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
    
    OLED_ShowString(1, 1, "Init...        ");
    ESP8266_Init();
    ESP_ConnectMQTT();
    OLED_ShowString(2, 1, "HX711 Init...  ");
    Init_HX711pin();
    
    OLED_ShowString(2, 1, "Tare...        ");
    Get_Maopi();
    tare_done = 1;
    
    OLED_Clear();
    OLED_ShowString(1, 1, "T:");
    OLED_ShowString(2, 1, "H:");
    OLED_ShowString(3, 1, "W:");

    ReadDHT11Data();
    Get_Weight();
    weight = Weight_Shiwu;
    UpdateDisplay();
    
    xReturn = xTaskCreate((TaskFunction_t)AppTaskCreate, "AppTaskCreate", 256, NULL, 1, &AppTaskCreate_Handle);
    
    if (pdPASS == xReturn)
    {
        vTaskStartScheduler();
    }
    else
    {
        OLED_ShowString(4, 1, "Create Fail");
    }
    
    while(1);
}
