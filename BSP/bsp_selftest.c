#include "bsp_selftest.h"
#include "bsp_log.h"

/* A non-destructive sanity check of a tiny scratch area on the current MSP.
 * Do not probe arbitrary SRAM addresses while the application stack is active. */
static uint8_t  s_failed;
static uint32_t s_first_bad;
static uint32_t s_first_bad_want;
static uint32_t s_first_bad_got;

static void fail(uint32_t addr, uint32_t want, uint32_t got)
{
    if (!s_failed) {
        s_failed         = 1u;
        s_first_bad      = addr;
        s_first_bad_want = want;
        s_first_bad_got  = got;
    }
}

uint8_t BSP_SelfTest_Run(void)
{
    volatile uint32_t scratch[4];
    static const uint32_t patterns[4] = {
        0x00000000UL, 0xFFFFFFFFUL, 0x55555555UL, 0xAAAAAAAAUL
    };
    uint32_t pass;
    uint32_t i;

    s_failed         = 0u;
    s_first_bad      = 0u;
    s_first_bad_want = 0u;
    s_first_bad_got  = 0u;

    for (pass = 0u; pass < 4u; pass++) {
        for (i = 0u; i < 4u; i++) {
            scratch[i] = patterns[pass] ^ (i * 0x00010001UL);
        }
        for (i = 0u; i < 4u; i++) {
            uint32_t want = patterns[pass] ^ (i * 0x00010001UL);
            uint32_t got = scratch[i];
            if (got != want) {
                fail((uint32_t)&scratch[i], want, got);
                return 0u;
            }
        }
    }

    return 1u;
}

uint8_t BSP_SelfTest_Failed(void)
{
    return s_failed;
}

/* Kept for API compatibility; no upper-SRAM scan is performed. */
uint32_t BSP_SelfTest_UpperOkBytes(void)
{
    return 0u;
}

void BSP_SelfTest_Report(void)
{
    BSP_Log_Puts("RAM SELF-CHECK: ");
    if (!s_failed) {
        BSP_Log_Puts("PASS (limited stack scratch check; no full SRAM sweep)\r\n");
    } else {
        BSP_Log_Puts("FAIL at ");
        BSP_Log_ShowAddr(s_first_bad);
        BSP_Log_Puts(" want ");
        BSP_Log_ShowAddr(s_first_bad_want);
        BSP_Log_Puts(" got ");
        BSP_Log_ShowAddr(s_first_bad_got);
        BSP_Log_Puts("\r\n");
    }
}
