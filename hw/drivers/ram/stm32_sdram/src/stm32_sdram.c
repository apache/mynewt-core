/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <string.h>
#include <os/mynewt.h>
#include <mcu/mcu.h>
#include <mcu/stm32_hal.h>
#include <arm_mpu.h>
#include <stm32_sdram/stm32_sdram.h>

#ifndef HAL_SDRAM_MODULE_ENABLED
#error "STM32_SDRAM requires HAL_SDRAM_MODULE_ENABLED in BSP stm32xxxx_hal_conf.h"
#endif

#define SDRAM_MODEREG_BURST_LENGTH_1          0x0000
#define SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL   0x0000
#define SDRAM_MODEREG_CAS_LATENCY_POS         4
#define SDRAM_MODEREG_OPERATING_MODE_STANDARD 0x0000
#define SDRAM_MODEREG_WRITEBURST_MODE_SINGLE  0x0200

#if MYNEWT_VAL(STM32_SDRAM_BANK) == 1
#define SDRAM_BANK       FMC_SDRAM_BANK1
#define SDRAM_CMD_TARGET FMC_SDRAM_CMD_TARGET_BANK1
#else
#define SDRAM_BANK       FMC_SDRAM_BANK2
#define SDRAM_CMD_TARGET FMC_SDRAM_CMD_TARGET_BANK2
#endif

#define SDRAM_SIZE MYNEWT_VAL(STM32_SDRAM_SIZE)

_Static_assert(MYNEWT_VAL(STM32_SDRAM_BANK) == 1 || MYNEWT_VAL(STM32_SDRAM_BANK) == 2,
               "STM32_SDRAM_BANK must be 1 or 2");
_Static_assert(SDRAM_SIZE >= (1 << 20) && SDRAM_SIZE <= (256 << 20) &&
                   (SDRAM_SIZE & (SDRAM_SIZE - 1)) == 0,
               "STM32_SDRAM_SIZE must be power of 2 between 1MB and 256MB");

/*
 * Linker symbols of .sdram output section (BSP/target user_sections.ld.h).
 * Weak so driver can be used without section (raw pointer access only).
 */
extern uint8_t __sdram_start__[] __attribute__((weak));
extern uint8_t __sdram_end__[] __attribute__((weak));

#if MYNEWT_VAL(STM32_SDRAM_MPU) && defined(ARM_MPU_ARMV7_H)

/*
 * Without MPU region SDRAM address range (0xC0000000-0xDFFFFFFF) is
 * Device memory, eXecute Never: no caching and unaligned access faults.
 * Region number follows element name order in mpu_regions link table.
 */
#if MYNEWT_VAL_CHOICE(STM32_SDRAM_MPU_ATTR, write_back)
/* Normal, write-back, read/write allocate */
#define SDRAM_MPU_TEX 1
#define SDRAM_MPU_C   1
#define SDRAM_MPU_B   1
#elif MYNEWT_VAL_CHOICE(STM32_SDRAM_MPU_ATTR, write_through)
/* Normal, write-through, no write allocate */
#define SDRAM_MPU_TEX 0
#define SDRAM_MPU_C   1
#define SDRAM_MPU_B   0
#else
/* Normal, non-cacheable */
#define SDRAM_MPU_TEX 1
#define SDRAM_MPU_C   0
#define SDRAM_MPU_B   0
#endif

/* ARM_MPU_REGION_SIZE_xx is log2(size) - 1 */
MPU_ARM_V7_REGION(stm32_sdram_region, STM32_SDRAM_BASE,
                  ARM_MPU_RASR(!MYNEWT_VAL(STM32_SDRAM_EXEC), ARM_MPU_AP_FULL,
                               SDRAM_MPU_TEX, 0, SDRAM_MPU_C, SDRAM_MPU_B, 0,
                               __builtin_ctz(SDRAM_SIZE) - 1));
#endif

static SDRAM_HandleTypeDef stm32_sdram_handle;
static uint32_t stm32_sdram_sdclk;
static uint8_t stm32_sdram_is_ready;

static void
stm32_sdram_delay_us(uint32_t us)
{
    /* Each iteration takes at least one cycle, delay is never shorter */
    volatile uint32_t n = (SystemCoreClock / 1000000) * us;

    while (n) {
        n--;
    }
}

/* FMC kernel clock frequency */
static uint32_t
stm32_sdram_fmc_clock(void)
{
#ifdef __HAL_RCC_GET_FMC_SOURCE
    PLL1_ClocksTypeDef pll1;
    PLL2_ClocksTypeDef pll2;

    switch (__HAL_RCC_GET_FMC_SOURCE()) {
    case RCC_FMCCLKSOURCE_PLL:
        HAL_RCCEx_GetPLL1ClockFreq(&pll1);
        return pll1.PLL1_Q_Frequency;
    case RCC_FMCCLKSOURCE_PLL2:
        HAL_RCCEx_GetPLL2ClockFreq(&pll2);
        return pll2.PLL2_R_Frequency;
    case RCC_FMCCLKSOURCE_HCLK:
        return HAL_RCC_GetHCLKFreq();
    default:
        /* CLKP (HSI/HSE/CSI) not supported */
        return 0;
    }
#else
    return HAL_RCC_GetHCLKFreq();
#endif
}

/* Convert ns to SDCLK cycles, rounding up, limited to 1..16 */
static uint32_t
stm32_sdram_cycles(uint32_t ns)
{
    uint32_t cycles;

    cycles = (uint32_t)(((uint64_t)ns * stm32_sdram_sdclk + 999999999U) / 1000000000U);

    return min(max(cycles, 1), 16);
}

static void
stm32_sdram_command(uint32_t mode, uint32_t refresh_cnt, uint32_t mode_reg)
{
    FMC_SDRAM_CommandTypeDef cmd = {
        .CommandMode = mode,
        .CommandTarget = SDRAM_CMD_TARGET,
        .AutoRefreshNumber = refresh_cnt,
        .ModeRegisterDefinition = mode_reg,
    };

    HAL_SDRAM_SendCommand(&stm32_sdram_handle, &cmd, HAL_MAX_DELAY);
#ifdef FMC_SDSR_BUSY
    /* F4/F7: command execution takes few SDCLK cycles, don't wait forever */
    for (uint32_t n = 100000;
         n && (stm32_sdram_handle.Instance->SDSR & FMC_SDSR_BUSY); --n) {
    }
#endif
}

size_t
stm32_sdram_size(void)
{
    const struct stm32_sdram_cfg *cfg = &os_bsp_sdram_cfg;

    return ((size_t)1 << (cfg->row_bits + cfg->col_bits)) *
           cfg->internal_banks * (cfg->bus_width / 8);
}

uint32_t
stm32_sdram_clock(void)
{
    return stm32_sdram_sdclk;
}

int
stm32_sdram_ready(void)
{
    return stm32_sdram_is_ready;
}

static int
stm32_sdram_check_cfg(const struct stm32_sdram_cfg *cfg)
{
    if (cfg->timing == NULL || cfg->col_bits < 8 || cfg->col_bits > 11 ||
        cfg->row_bits < 11 || cfg->row_bits > 13 ||
        (cfg->internal_banks != 2 && cfg->internal_banks != 4) ||
        (cfg->bus_width != 8 && cfg->bus_width != 16 && cfg->bus_width != 32) ||
        cfg->cas_latency < 1 || cfg->cas_latency > 3 ||
        (cfg->sdclk_div != 2 && cfg->sdclk_div != 3) || cfg->read_pipe_delay > 2) {
        return -1;
    }
    /* MPU region and linker region use STM32_SDRAM_SIZE */
    if (stm32_sdram_size() > SDRAM_SIZE) {
        return -1;
    }
    if (__sdram_end__ != NULL &&
        (uintptr_t)__sdram_end__ > STM32_SDRAM_BASE + stm32_sdram_size()) {
        return -1;
    }

    return 0;
}

static void
stm32_sdram_fill_init(const struct stm32_sdram_cfg *cfg)
{
    static const uint32_t col_bits[] = {
        FMC_SDRAM_COLUMN_BITS_NUM_8,
        FMC_SDRAM_COLUMN_BITS_NUM_9,
        FMC_SDRAM_COLUMN_BITS_NUM_10,
        FMC_SDRAM_COLUMN_BITS_NUM_11,
    };
    static const uint32_t row_bits[] = {
        FMC_SDRAM_ROW_BITS_NUM_11,
        FMC_SDRAM_ROW_BITS_NUM_12,
        FMC_SDRAM_ROW_BITS_NUM_13,
    };
    static const uint32_t cas[] = {
        FMC_SDRAM_CAS_LATENCY_1,
        FMC_SDRAM_CAS_LATENCY_2,
        FMC_SDRAM_CAS_LATENCY_3,
    };
    static const uint32_t rpipe[] = {
        FMC_SDRAM_RPIPE_DELAY_0,
        FMC_SDRAM_RPIPE_DELAY_1,
        FMC_SDRAM_RPIPE_DELAY_2,
    };
    FMC_SDRAM_InitTypeDef *init = &stm32_sdram_handle.Init;

    stm32_sdram_handle.Instance = FMC_SDRAM_DEVICE;
    init->SDBank = SDRAM_BANK;
    init->ColumnBitsNumber = col_bits[cfg->col_bits - 8];
    init->RowBitsNumber = row_bits[cfg->row_bits - 11];
    init->MemoryDataWidth = cfg->bus_width == 8 ? FMC_SDRAM_MEM_BUS_WIDTH_8
                            : cfg->bus_width == 16 ? FMC_SDRAM_MEM_BUS_WIDTH_16
                                                   : FMC_SDRAM_MEM_BUS_WIDTH_32;
    init->InternalBankNumber = cfg->internal_banks == 2
                                   ? FMC_SDRAM_INTERN_BANKS_NUM_2
                                   : FMC_SDRAM_INTERN_BANKS_NUM_4;
    init->CASLatency = cas[cfg->cas_latency - 1];
    init->WriteProtection = FMC_SDRAM_WRITE_PROTECTION_DISABLE;
    init->SDClockPeriod = cfg->sdclk_div == 2 ? FMC_SDRAM_CLOCK_PERIOD_2
                                              : FMC_SDRAM_CLOCK_PERIOD_3;
    init->ReadBurst =
        cfg->read_burst ? FMC_SDRAM_RBURST_ENABLE : FMC_SDRAM_RBURST_DISABLE;
    init->ReadPipeDelay = rpipe[cfg->read_pipe_delay];
}

static void
stm32_sdram_fill_timing(const struct stm32_sdram_timing *t,
                        FMC_SDRAM_TimingTypeDef *timing)
{
    uint32_t rc = stm32_sdram_cycles(t->t_rc_ns);
    uint32_t ras = stm32_sdram_cycles(t->t_ras_ns);
    uint32_t rp = stm32_sdram_cycles(t->t_rp_ns);
    uint32_t rcd = stm32_sdram_cycles(t->t_rcd_ns);
    uint32_t wr = min(max(t->t_wr_clk, 1), 16);

    /* FMC requires TWR >= TRAS - TRCD and TWR >= TRC - TRCD - TRP */
    if (ras > rcd) {
        wr = max(wr, ras - rcd);
    }
    if (rc > rcd + rp) {
        wr = max(wr, rc - rcd - rp);
    }

    timing->LoadToActiveDelay = min(max(t->t_mrd_clk, 1), 16);
    timing->ExitSelfRefreshDelay = stm32_sdram_cycles(t->t_xsr_ns);
    timing->SelfRefreshTime = ras;
    timing->RowCycleDelay = rc;
    timing->WriteRecoveryTime = wr;
    timing->RPDelay = rp;
    timing->RCDDelay = rcd;
}

static void
stm32_sdram_clear(void)
{
    uintptr_t start = (uintptr_t)__sdram_start__;
    uintptr_t end = (uintptr_t)__sdram_end__;

    if (start != 0 && end > start) {
        memset((void *)start, 0, end - start);
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
        if (SCB->CCR & SCB_CCR_DC_Msk) {
            SCB_CleanDCache();
        }
#endif
    }
}

static void
stm32_sdram_cache_flush(void)
{
#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    if (SCB->CCR & SCB_CCR_DC_Msk) {
        SCB_CleanInvalidateDCache();
    }
#endif
}

int
stm32_sdram_test(uint32_t start, size_t size, uint32_t *fail_addr)
{
    volatile uint32_t *mem = (volatile uint32_t *)start;
    size_t words = size / 4;
    uint32_t pattern;
    size_t offset;
    size_t i;
    uint32_t bad = 0;

    if (start < STM32_SDRAM_BASE || (start & 3) || words == 0 ||
        start + size > STM32_SDRAM_BASE + stm32_sdram_size()) {
        bad = start;
        goto fail;
    }

    /* Data bus: walking ones on first word */
    for (pattern = 1; pattern; pattern <<= 1) {
        mem[0] = pattern;
        stm32_sdram_cache_flush();
        if (mem[0] != pattern) {
            bad = start;
            goto fail;
        }
    }

    /* Address bus: power of two word offsets */
    for (offset = 1; offset < words; offset <<= 1) {
        mem[offset] = 0xAAAAAAAA;
    }
    mem[0] = 0x55555555;
    stm32_sdram_cache_flush();
    for (offset = 1; offset < words; offset <<= 1) {
        if (mem[offset] != 0xAAAAAAAA) {
            bad = (uint32_t)&mem[offset];
            goto fail;
        }
    }

    /* Every word: address as data, then inverted */
    for (i = 0; i < words; ++i) {
        mem[i] = (uint32_t)&mem[i];
    }
    stm32_sdram_cache_flush();
    for (i = 0; i < words; ++i) {
        if (mem[i] != (uint32_t)&mem[i]) {
            bad = (uint32_t)&mem[i];
            goto fail;
        }
        mem[i] = ~(uint32_t)&mem[i];
    }
    stm32_sdram_cache_flush();
    for (i = 0; i < words; ++i) {
        if (mem[i] != ~(uint32_t)&mem[i]) {
            bad = (uint32_t)&mem[i];
            goto fail;
        }
    }

    return 0;
fail:
    if (fail_addr) {
        *fail_addr = bad;
    }
    return -1;
}

void
stm32_sdram_init(void)
{
    const struct stm32_sdram_cfg *cfg = &os_bsp_sdram_cfg;
    FMC_SDRAM_TimingTypeDef timing;
    uint32_t fmc_clk;
    uint32_t refresh;
    int rc;
    int i;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    rc = stm32_sdram_check_cfg(cfg);
    SYSINIT_PANIC_ASSERT_MSG(rc == 0, "Invalid SDRAM configuration");

    fmc_clk = stm32_sdram_fmc_clock();
    SYSINIT_PANIC_ASSERT_MSG(fmc_clk != 0, "Unsupported FMC clock source");
    stm32_sdram_sdclk = fmc_clk / cfg->sdclk_div;

    for (i = 0; i < cfg->pin_cnt; ++i) {
        rc = hal_gpio_init_fun(cfg->pins[i]);
        SYSINIT_PANIC_ASSERT(rc == 0);
    }

    __HAL_RCC_FMC_CLK_ENABLE();

    stm32_sdram_fill_init(cfg);
    stm32_sdram_fill_timing(cfg->timing, &timing);
    rc = HAL_SDRAM_Init(&stm32_sdram_handle, &timing);
    SYSINIT_PANIC_ASSERT(rc == HAL_OK);

    /* JEDEC power up sequence */
    stm32_sdram_command(FMC_SDRAM_CMD_CLK_ENABLE, 1, 0);
    /* At least 100 us with stable clock before any command but NOP */
    stm32_sdram_delay_us(MYNEWT_VAL(STM32_SDRAM_POWERUP_DELAY_US));
    stm32_sdram_command(FMC_SDRAM_CMD_PALL, 1, 0);
    stm32_sdram_command(FMC_SDRAM_CMD_AUTOREFRESH_MODE, 8, 0);
    stm32_sdram_command(FMC_SDRAM_CMD_LOAD_MODE, 1,
                        SDRAM_MODEREG_BURST_LENGTH_1 |
                            SDRAM_MODEREG_BURST_TYPE_SEQUENTIAL |
                            (cfg->cas_latency << SDRAM_MODEREG_CAS_LATENCY_POS) |
                            SDRAM_MODEREG_OPERATING_MODE_STANDARD |
                            SDRAM_MODEREG_WRITEBURST_MODE_SINGLE);

    /* Refresh counter: (refresh period / number of rows) * SDCLK - 20 */
    refresh = (uint32_t)((uint64_t)stm32_sdram_sdclk * cfg->timing->t_refresh_ms /
                         1000 / (1U << cfg->row_bits)) -
              20;
    rc = HAL_SDRAM_ProgramRefreshRate(&stm32_sdram_handle, refresh);
    SYSINIT_PANIC_ASSERT(rc == HAL_OK);

    stm32_sdram_is_ready = 1;

#if MYNEWT_VAL(STM32_SDRAM_TEST)
    rc = stm32_sdram_test(STM32_SDRAM_BASE, stm32_sdram_size(), NULL);
    SYSINIT_PANIC_ASSERT_MSG(rc == 0, "SDRAM test failed");
#endif

    if (MYNEWT_VAL(STM32_SDRAM_CLEAR)) {
        stm32_sdram_clear();
    }
}
