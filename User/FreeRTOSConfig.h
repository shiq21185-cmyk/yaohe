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
/* 空闲任务栈：128 字(512B) -> 64 字(256B)。
 * prvIdleTask 的静态最大深度只有 100 字节，空闲钩子 vApplicationIdleHook()
 * 里只调用 BSP_Log_* 这套纯寄存器级输出（不走 printf/sprintf，最大局部
 * 缓冲是 BSP_Log_Dec 的 11 字节），所以 256B 足够。 */
#define configMINIMAL_STACK_SIZE    ( ( unsigned short ) 64 )
/* 内核堆已经没有任何分配者：任务栈/TCB 走静态分配，
 * 两个互斥量（xOLEDMutex / xTimeMutex）也都改成了 Static 版本，
 * 语音信号量随 USART3 中断改造（ISR 只收字节、任务轮询组帧）一并移除，
 * 软件定时器已关闭。这里只留 128 字节兜底：
 * heap_4 的 prvHeapInit() 需要一个能放下块头(8B)加一个空闲块的极小空间，
 * 万一将来有代码偷偷调用 pvPortMalloc，vApplicationMallocFailedHook()
 * 会在串口打印 [MALLOC-FAILED] 而不是悄悄跑飞。 */
#define configTOTAL_HEAP_SIZE       ( ( size_t ) 128 )
#define configMAX_TASK_NAME_LEN     ( 16 )
#define configUSE_TRACE_FACILITY    0
#define configUSE_16_BIT_TICKS      0
#define configIDLE_SHOULD_YIELD     1

/* 静态内存、同步对象配置。
 * 本工程没有用到任何软件定时器（全文搜不到 xTimerCreate/xTimerStart），
 * 关掉 configUSE_TIMERS 可以整整省下定时器任务的栈、TCB、定时器队列和
 * 两条定时器链表，约 800 字节 RAM；timers.c 会被整体编译为空。 */
#define configSUPPORT_STATIC_ALLOCATION 1
#define configUSE_TIMERS                0
#if ( configUSE_TIMERS == 1 )
#define configTIMER_TASK_PRIORITY       2
#define configTIMER_QUEUE_LENGTH        5
#define configTIMER_TASK_STACK_DEPTH    configMINIMAL_STACK_SIZE
#endif
#define configUSE_MUTEXES               1
/* 递归互斥量与计数信号量都没有被使用（只有互斥量和二值信号量），
 * 关掉可减小内核代码体积。 */
#define configUSE_RECURSIVE_MUTEXES     0
#define configUSE_COUNTING_SEMAPHORES   0
/* 队列注册表（vQueueAddToRegistry）没有被使用，置 0 省下 64 字节数组。 */
#define configQUEUE_REGISTRY_SIZE       0

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
