#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

#include "stm32f10x.h"

/* 调度器与系统时钟配置。 */
#define configUSE_PREEMPTION        1
#define configUSE_IDLE_HOOK         1
#define configUSE_TICK_HOOK         0
#define configCPU_CLOCK_HZ          ( ( unsigned long ) 72000000 )
#define configTICK_RATE_HZ          ( ( TickType_t ) 1000 )
#define configMAX_PRIORITIES        ( 5 )
/* 空闲任务栈：128 字 = 512 字节（FreeRTOS 常规取值）。 */
#define configMINIMAL_STACK_SIZE    ( ( unsigned short ) 128 )
/* 内核堆大小：20KB。
 * 本工程采用大众化用法——所有任务用 xTaskCreate() 动态创建，
 * 互斥量/信号量用 xSemaphoreCreateMutex() 这类动态 API，
 * 它们的栈、TCB、内核对象全部从这个堆里切出来。
 * 实际开销约 16KB（9 个任务栈按 app_tasks.h 的 512/256 字取值、
 * 加上空闲任务与定时器任务、各自的 TCB、分配块头与定时器队列），
 * 留 4KB 余量后取 20KB。芯片 SRAM 是 48KB
 * （Poject.uvprojx 的 IRAM 已配为 0xc000），装得下。 */
#define configTOTAL_HEAP_SIZE       ( ( size_t ) ( 20 * 1024 ) )
#define configMAX_TASK_NAME_LEN     ( 16 )
#define configUSE_TRACE_FACILITY    0
#define configUSE_16_BIT_TICKS      0
#define configIDLE_SHOULD_YIELD     1

/* 内存与同步对象配置：恢复 FreeRTOS 默认的大众用法。
 * 任务、互斥量、信号量一律走动态分配（xTaskCreate / xSemaphoreCreateMutex /
 * xSemaphoreCreateBinary），因此关闭静态分配——这样内核自己只用堆来创建
 * 空闲任务，不需要应用再提供 vApplicationGetIdleTaskMemory() 这类回调。 */
#define configSUPPORT_STATIC_ALLOCATION 0
#define configUSE_TIMERS                1
#if ( configUSE_TIMERS == 1 )
#define configTIMER_TASK_PRIORITY       2
#define configTIMER_QUEUE_LENGTH        5
#define configTIMER_TASK_STACK_DEPTH    configMINIMAL_STACK_SIZE
#endif
#define configUSE_MUTEXES               1
#define configUSE_RECURSIVE_MUTEXES     1
#define configUSE_COUNTING_SEMAPHORES   1
#define configQUEUE_REGISTRY_SIZE       8

/* 栈溢出检测与断言。
 * 方法2：切换上下文时校验栈末尾 20 字节仍为 0xA5 填充值，能抓到最危险的
 * 越界写。溢出会调用 vApplicationStackOverflowHook()（见 app_tasks.c），
 * 通过 USART1 打印任务名并停机。
 * 注意：本工程所有中断服务函数都不调用 FreeRTOS 的 ...FromISR API，因此
 * configASSERT 里的中断优先级校验不会被触发。 */
#define configCHECK_FOR_STACK_OVERFLOW  2
#define configUSE_MALLOC_FAILED_HOOK    1

void vAssertCalled(const char *pcFile, unsigned long ulLine);
#define configASSERT( x ) \
    if( ( x ) == 0 ) { vAssertCalled( __FILE__, __LINE__ ); }

/* 本工程使用的可选 FreeRTOS API。 */
#define INCLUDE_xTaskGetTickCount       1
#define INCLUDE_vTaskDelayUntil         1
#define INCLUDE_vTaskPrioritySet        1
#define INCLUDE_uxTaskPriorityGet       1
#define INCLUDE_vTaskDelete             1
#define INCLUDE_vTaskCleanUpResources   0
#define INCLUDE_vTaskSuspend            1
#define INCLUDE_vTaskDelay              1
#define INCLUDE_xTaskGetSchedulerState  1
#define INCLUDE_uxTaskGetStackHighWaterMark 1

/* Cortex-M3 异常处理函数映射与中断优先级。 */
#define vPortSVCHandler SVC_Handler
#define xPortPendSVHandler PendSV_Handler
#define configUSE_CO_ROUTINES 0
#define configMAX_CO_ROUTINE_PRIORITIES 2
#define configKERNEL_INTERRUPT_PRIORITY 255
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 191
#define configLIBRARY_KERNEL_INTERRUPT_PRIORITY 15

#endif
