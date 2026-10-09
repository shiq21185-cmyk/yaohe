#ifndef __BSP_FLASH_H
#define __BSP_FLASH_H

#include <stdint.h>

/* ==========================================================================
 * 内部 Flash 语义层
 * --------------------------------------------------------------------------
 * 只服务于"药盒闹钟掉电保存"这一件事：把三组闹钟打包成 5 个半字
 * （magic + 3 组数据 + 校验和）写进 STM32F103C8T6 的最后一个 1KB 页。
 * 擦除/编程/解锁这些动作全部收敛在这里，驱动层只说"存闹钟/读闹钟"。
 * ========================================================================== */

#define BSP_FLASH_ALARM_ADDR        0x0800FC00UL   /* 64KB Flash 的最后一页 */
#define BSP_FLASH_ALARM_MAGIC       0xA65AU
#define BSP_FLASH_ALARM_WORD_COUNT  5U
#define BSP_FLASH_ALARM_XOR_KEY     0x5AA5U

/* 校验和 = word0 ^ word1 ^ word2 ^ word3 ^ BSP_FLASH_ALARM_XOR_KEY，
 * 调用前请先填好 words[0..3]。 */
uint16_t BSP_Flash_AlarmChecksum(const uint16_t *words);

/* 从 Flash 读出 5 个半字。返回 1 表示 magic 与校验和都匹配。 */
uint8_t BSP_Flash_ReadAlarms(uint16_t *words);

/* 擦除闹钟页并写入 5 个半字。返回 1 表示擦除与全部编程都成功。 */
uint8_t BSP_Flash_WriteAlarms(const uint16_t *words);

#endif
