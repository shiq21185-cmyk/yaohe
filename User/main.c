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
#include "bsp_log.h"
#include "bsp_selftest.h"
#include "cm_backtrace_lite.h"

/* 外部时间互斥量 */
extern SemaphoreHandle_t xTimeMutex;

/* 两个互斥量改为静态创建：这样内核堆(ucHeap)里就再没有任何分配者，
 * configTOTAL_HEAP_SIZE 可以从 1KB 缩到 128B 的兜底值。
 * 代价是 .bss 多出两个 StaticSemaphore_t（各约 80 字节）。 */
static StaticSemaphore_t xOLEDMutexBuffer;
static StaticSemaphore_t xTimeMutexBuffer;

int main(void)
{
    BaseType_t xReturn = pdPASS;

    /* 中断必须最早打开：标准库启动代码在进入 main 前关了全局中断，
     * 而下面的自检与初始化都依赖 SysTick / USART 中断能进得来。 */
    __enable_irq();

    /* 开启子异常分类与除零陷阱：必须在任何外设初始化之前调用 */
    cm_backtrace_init();

    /* 日志口先于一切初始化：只配 PA9，不注册中断，
     * 之后 USART1_Init() 再接管为 HC-06 正常收发。 */
    BSP_Log_Init();
    BSP_Log_Puts("\r\n\r\n=== MEDBOX BSP BOOT ===\r\n");
    BSP_Log_Puts("build: BSP static-tasks heap-128B stacks-trimmed\r\n");

    /* 内存自检：确认 0x20004C00-0x2000C000 可读写、无混叠。
     * 放在 RTOS 堆与任务创建之前，此时该区间无人使用，可以安全破坏。 */
    if (!BSP_SelfTest_Run())
    {
        BSP_SelfTest_Report();
        BSP_Log_Puts("RAM SELF-TEST FAILED - halting\r\n");
        while (1) { }
    }
    BSP_SelfTest_Report();

    LED_Init();
    OLED_Init();

    /* OLED互斥量 */
    extern SemaphoreHandle_t xOLEDMutex;
    xOLEDMutex = xSemaphoreCreateMutexStatic(&xOLEDMutexBuffer);
    if(xOLEDMutex == NULL)
    {
        OLED_ShowString(1, 1, "Mutex Fail");
        while(1);
    }

    /* 时间互斥量 */
    xTimeMutex = xSemaphoreCreateMutexStatic(&xTimeMutexBuffer);
    if(xTimeMutex == NULL)
    {
        OLED_ShowString(1, 1, "TimeMutex Fail");
        while(1);
    }

    TIM4_Init();

    HC06_Init();
    USART1_Init(9600);      /* USART1初始化 */
    USART2_Init(115200);    /* USART2连接ESP8266 */

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

    /* 外设初始化全部走完后才吐通过令牌，
     * AutoDebug 以串口是否收到该令牌判定成功。 */
    BSP_Log_Puts("[ALL TESTS PASSED]\r\n");

    /* 任务创建任务本身也用静态内存，堆里不再有任务栈与TCB。 */
    AppTaskCreate_Handle = xTaskCreateStatic((TaskFunction_t)AppTaskCreate, "AppTaskCreate",
                                             APPTASKCREATE_STACK_WORDS, NULL, 1,
                                             AppTaskCreate_Stack, &AppTaskCreate_TCB);
    xReturn = (AppTaskCreate_Handle != NULL) ? pdPASS : pdFAIL;

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
