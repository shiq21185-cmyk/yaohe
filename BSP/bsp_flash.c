#include "bsp_flash.h"
#include "stm32f10x.h"
#include "stm32f10x_flash.h"

/* ==========================================================================
 * 内部 Flash 语义层实现
 * ========================================================================== */

uint16_t BSP_Flash_AlarmChecksum(const uint16_t *words)
{
    return (uint16_t)(words[0] ^ words[1] ^ words[2] ^ words[3] ^ BSP_FLASH_ALARM_XOR_KEY);
}

uint8_t BSP_Flash_ReadAlarms(uint16_t *words)
{
    const volatile uint16_t *flash = (const volatile uint16_t *)BSP_FLASH_ALARM_ADDR;
    uint8_t i;

    for (i = 0; i < BSP_FLASH_ALARM_WORD_COUNT; i++)
    {
        words[i] = flash[i];
    }

    if (words[0] != BSP_FLASH_ALARM_MAGIC) { return 0; }
    if (words[4] != BSP_Flash_AlarmChecksum(words)) { return 0; }

    return 1;
}

uint8_t BSP_Flash_WriteAlarms(const uint16_t *words)
{
    uint8_t i;
    uint8_t ok = 1;

    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);

    if (FLASH_ErasePage(BSP_FLASH_ALARM_ADDR) != FLASH_COMPLETE)
    {
        ok = 0;
    }
    else
    {
        for (i = 0; i < BSP_FLASH_ALARM_WORD_COUNT; i++)
        {
            if (FLASH_ProgramHalfWord(BSP_FLASH_ALARM_ADDR + (uint32_t)i * 2U, words[i]) != FLASH_COMPLETE)
            {
                ok = 0;
                break;
            }
        }
    }

    FLASH_Lock();
    return ok;
}
