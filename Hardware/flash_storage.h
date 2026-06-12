
#ifndef __FLASH_STORAGE_H
#define __FLASH_STORAGE_H

#include "stm32f10x.h"

// Flash存储地址 - 使用Flash末尾区域
// STM32F103C8有64KB Flash (0x08000000 - 0x08010000)
// 使用最后一页(0x0800FC00 - 0x08010000)来存储数据
#define FLASH_STORAGE_ADDR  0x0800FC00  // Flash存储起始地址
#define FLASH_PAGE_SIZE     0x400       // 1KB一页

// 数据校验魔数
#define STORAGE_MAGIC       0x5A5AA5A5

// 闹钟数据结构（独立定义，避免循环依赖）
typedef struct {
    uint8_t hour;      // 闹钟小时 (0-23)
    uint8_t minute;    // 闹钟分钟 (0-59)
    uint8_t enabled;   // 闹钟使能标志 (1=启用, 0=禁用)
} FlashAlarm_t;

// 存储数据结构
typedef struct {
    uint32_t magic;                 // 魔数，用于校验数据有效性
    FlashAlarm_t alarms[3];         // 3个闹钟数据
    uint16_t checksum;              // 校验和
} FlashStorage_t;

// 函数声明
void FlashStorage_Init(void);
uint8_t FlashStorage_SaveAlarms(const FlashAlarm_t *alarms);
uint8_t FlashStorage_LoadAlarms(FlashAlarm_t *alarms);
uint8_t FlashStorage_IsValid(void);

#endif

