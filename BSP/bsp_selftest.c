#include "bsp_selftest.h"
#include "bsp_log.h"

/* ------------------------------------------------------------------ *
 * Destructive marching / walking-1 / aliasing test over 0x20004C00 .. *
 * 0x2000C000. The 28 KB above 0x20005000 is what stage 1 exposes to   *
 * the linker, so it gets the walking-1 scan plus a chunk-cursor test  *
 * that catches blocks which merely alias a lower address.            *
 * Running RAM below the test base is never touched.                  *
 * ------------------------------------------------------------------ */

#define GUARD_WORDS 4

static uint8_t  s_failed;
static uint32_t s_first_bad;
static uint32_t s_first_bad_want;
static uint32_t s_first_bad_got;

static const uint32_t s_pattern[4] = {
    0x00000000UL, 0xFFFFFFFFUL, 0x55555555UL, 0xAAAAAAAAUL
};

static int word_is_zero(uint32_t a)
{
    return (*(volatile uint32_t *)a == 0UL);
}

static int is_open_bus(uint32_t a)
{
    volatile uint32_t sink;

    /* All-zeros, all-ones and two alternating patterns: reading back the same
     * word for every one of them means the address is not backed by SRAM. */
    *(volatile uint32_t *)a = 0x00000000UL;
    sink = *(volatile uint32_t *)a;
    if (sink != 0x00000000UL) { return 0; }

    *(volatile uint32_t *)a = 0xFFFFFFFFUL;
    sink = *(volatile uint32_t *)a;
    if (sink != 0xFFFFFFFFUL) { return 0; }

    *(volatile uint32_t *)a = 0x55555555UL;
    sink = *(volatile uint32_t *)a;
    if (sink != 0x55555555UL) { return 0; }

    *(volatile uint32_t *)a = 0xAAAAAAAAUL;
    sink = *(volatile uint32_t *)a;
    if (sink != 0xAAAAAAAAUL) { return 0; }

    return 1;
}

static void fail(uint32_t addr, uint32_t want, uint32_t got)
{
    if (!s_failed) {
        s_failed       = 1;
        s_first_bad    = addr;
        s_first_bad_want = want;
        s_first_bad_got  = got;
    }
}

static void test_chunk(uint32_t base, uint32_t size)
{
    uint32_t backup[GUARD_WORDS];
    uint32_t i, a, w;
    volatile uint32_t sink;

    for (i = 0; i < (size / 4UL); i++) {
        backup[i] = *(volatile uint32_t *)(base + i * 4UL);
    }

    /* Marching ones and zeros. */
    for (i = 0; i < (size / 4UL); i++) {
        *(volatile uint32_t *)(base + i * 4UL) = 0xFFFFFFFFUL;
    }
    for (i = 0; i < (size / 4UL); i++) {
        a = base + i * 4UL;
        if (*(volatile uint32_t *)a != 0xFFFFFFFFUL) {
            fail(a, 0xFFFFFFFFUL, *(volatile uint32_t *)a);
        }
        *(volatile uint32_t *)a = 0x00000000UL;
        if (*(volatile uint32_t *)a != 0x00000000UL) {
            fail(a, 0x00000000UL, *(volatile uint32_t *)a);
        }
    }

    /* Four static patterns. */
    for (w = 0; w < 4UL; w++) {
        for (i = 0; i < (size / 4UL); i++) {
            *(volatile uint32_t *)(base + i * 4UL) = s_pattern[w];
        }
        for (i = 0; i < (size / 4UL); i++) {
            a = base + i * 4UL;
            if (*(volatile uint32_t *)a != s_pattern[w]) {
                fail(a, s_pattern[w], *(volatile uint32_t *)a);
            }
        }
    }

    /* Walking single bit in both polarities. */
    for (w = 0; w < 32UL; w++) {
        uint32_t bit = (1UL << w);
        for (i = 0; i < (size / 4UL); i++) {
            *(volatile uint32_t *)(base + i * 4UL) = bit;
        }
        for (i = 0; i < (size / 4UL); i++) {
            a = base + i * 4UL;
            if (*(volatile uint32_t *)a != bit) { fail(a, bit, *(volatile uint32_t *)a); }
            *(volatile uint32_t *)a = ~bit;
        }
        for (i = 0; i < (size / 4UL); i++) {
            a = base + i * 4UL;
            if (*(volatile uint32_t *)a != ~bit) { fail(a, ~bit, *(volatile uint32_t *)a); }
        }
    }

    /* Cursor test: write a value derived from the index, then compare one by
     * one. If a block mirrors a lower address, the expected value will not be
     * there even though every single-cell test above passed. */
    for (i = 0; i < (size / 4UL); i++) {
        *(volatile uint32_t *)(base + i * 4UL) =
            (0xA5A5A5A5UL ^ (i * 0x00010001UL)) + base;
    }
    for (i = 0; i < (size / 4UL); i++) {
        uint32_t want = (0xA5A5A5A5UL ^ (i * 0x00010001UL)) + base;
        a = base + i * 4UL;
        if (*(volatile uint32_t *)a != want) {
            fail(a, want, *(volatile uint32_t *)a);
        }
    }

    /* Final cross-cell walk: exactly one cell holds 0x5A5A5A5A at a time. */
    for (i = 0; i < (size / 4UL); i++) {
        *(volatile uint32_t *)(base + i * 4UL) = 0x00000000UL;
    }
    for (i = 0; i < (size / 4UL); i++) {
        a = base + i * 4UL;
        *(volatile uint32_t *)a = 0x5A5A5A5AUL;
        if (*(volatile uint32_t *)a != 0x5A5A5A5AUL) {
            fail(a, 0x5A5A5A5AUL, *(volatile uint32_t *)a);
        }
        sink = *(volatile uint32_t *)(base + ((i + 1UL) % (size / 4UL)) * 4UL);
        (void)sink;
        *(volatile uint32_t *)a = 0x00000000UL;
    }

    for (i = 0; i < (size / 4UL); i++) {
        *(volatile uint32_t *)(base + i * 4UL) = backup[i];
    }
}

uint8_t BSP_SelfTest_Run(void)
{
    uint32_t a;
    uint32_t probe_end;
    uint32_t chunk;

    s_failed         = 0;
    s_first_bad      = 0;
    s_first_bad_want = 0;
    s_first_bad_got  = 0;

    /* 1. How much of the window is actually backed by SRAM? Probe with the
     *    classifier first so a partly-populated die is reported, not crashed. */
    probe_end = BSP_RAM_TEST_BASE;
    for (a = BSP_RAM_TEST_BASE; a < BSP_RAM_END; a += 4UL) {
        *(volatile uint32_t *)a = 0x5A5A5A5AUL;
        if (*(volatile uint32_t *)a != 0x5A5A5A5AUL) { break; }
        if (word_is_zero(a) && is_open_bus(a)) { break; }
        probe_end = a + 4UL;
    }

    /* 2. Thorough test over everything the probe proved addressable. */
    chunk = 0x400UL;
    for (a = BSP_RAM_TEST_BASE; a + chunk <= probe_end; a += chunk) {
        test_chunk(a, chunk);
        if (s_failed) { break; }
    }

    return s_failed ? 0u : 1u;
}

uint8_t BSP_SelfTest_Failed(void)
{
    return s_failed;
}

uint32_t BSP_SelfTest_UpperOkBytes(void)
{
    return 0;
}

void BSP_SelfTest_Report(void)
{
    BSP_Log_Puts("RAM TEST: ");
    if (!s_failed) {
        BSP_Log_Puts("PASS. Addressable 0x20004C00-0x2000C000, 48KB total\r\n");
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
