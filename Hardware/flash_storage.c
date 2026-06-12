#include "flash_storage.h"
#include "stm32f10x_flash.h"

static uint16_t CalculateChecksum(const FlashStorage_t *data)
{
    uint16_t sum = 0;
    const uint8_t *ptr = (const uint8_t *)data;
    uint32_t size = sizeof(FlashStorage_t) - sizeof(uint16_t);
    
    for(uint32_t i = 0; i < size; i++)
    {
        sum += ptr[i];
    }
    return sum;
}

void FlashStorage_Init(void)
{
    FLASH_Unlock();
    FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
}

uint8_t FlashStorage_IsValid(void)
{
    const FlashStorage_t *stored = (const FlashStorage_t *)FLASH_STORAGE_ADDR;
    
    if(stored->magic != STORAGE_MAGIC)
    {
        return 0;
    }
    
    uint16_t calcChecksum = CalculateChecksum(stored);
    if(stored->checksum != calcChecksum)
    {
        return 0;
    }
    
    return 1;
}

uint8_t FlashStorage_SaveAlarms(const FlashAlarm_t *alarms)
{
    FlashStorage_t data;
    FLASH_Status status;
    uint32_t addr;
    uint16_t *dataPtr;
    
    data.magic = STORAGE_MAGIC;
    for(int i = 0; i < 3; i++)
    {
        data.alarms[i] = alarms[i];
    }
    data.checksum = CalculateChecksum(&data);
    
    FLASH_Unlock();
    
    status = FLASH_ErasePage(FLASH_STORAGE_ADDR);
    if(status != FLASH_COMPLETE)
    {
        FLASH_Lock();
        return 0;
    }
    
    addr = FLASH_STORAGE_ADDR;
    dataPtr = (uint16_t *)&data;
    uint32_t halfWordCount = sizeof(FlashStorage_t) / 2;
    
    for(uint32_t i = 0; i < halfWordCount; i++)
    {
        status = FLASH_ProgramHalfWord(addr, dataPtr[i]);
        if(status != FLASH_COMPLETE)
        {
            FLASH_Lock();
            return 0;
        }
        addr += 2;
    }
    
    FLASH_Lock();
    
    return 1;
}

uint8_t FlashStorage_LoadAlarms(FlashAlarm_t *alarms)
{
    const FlashStorage_t *stored = (const FlashStorage_t *)FLASH_STORAGE_ADDR;
    
    if(!FlashStorage_IsValid())
    {
        return 0;
    }
    
    for(int i = 0; i < 3; i++)
    {
        alarms[i] = stored->alarms[i];
    }
    
    return 1;
}

