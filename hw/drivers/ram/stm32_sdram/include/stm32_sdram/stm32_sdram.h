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

#ifndef __STM32_SDRAM_H__
#define __STM32_SDRAM_H__

#include <stddef.h>
#include <stdint.h>
#include <syscfg/syscfg.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Base address of SDRAM bank selected by STM32_SDRAM_BANK */
#if MYNEWT_VAL(STM32_SDRAM_BANK) == 1
#define STM32_SDRAM_BASE 0xC0000000UL
#else
#define STM32_SDRAM_BASE 0xD0000000UL
#endif

/*
 * Place variable in external SDRAM.
 * Section is NOLOAD: there is no initial value. It is zeroed by
 * stm32_sdram_init() when STM32_SDRAM_CLEAR is set, so it behaves like .bss.
 * BSP (or target) linker include must provide .sdram output section.
 */
#define STM32_SDRAM_SECTION __attribute__((section(".sdram")))

/*
 * SDRAM chip timing (from chip data sheet, for the used speed grade
 * and CAS latency). Values in ns are converted to FMC clock cycles at run
 * time, rounding up.
 */
struct stm32_sdram_timing {
    /* Load mode register to active delay (tMRD) in clock cycles */
    uint8_t t_mrd_clk;
    /* Write recovery time (tWR / tDPL) in clock cycles */
    uint8_t t_wr_clk;
    /* Exit self-refresh to active (tXSR) in ns */
    uint16_t t_xsr_ns;
    /* Minimum self-refresh period, row active time (tRAS) in ns */
    uint16_t t_ras_ns;
    /* Row cycle time (tRC) in ns */
    uint16_t t_rc_ns;
    /* Row precharge time (tRP) in ns */
    uint16_t t_rp_ns;
    /* Row to column delay (tRCD) in ns */
    uint16_t t_rcd_ns;
    /* Refresh period of all rows in ms (usually 64) */
    uint16_t t_refresh_ms;
};

/* SDRAM chip and board configuration, provided by BSP */
struct stm32_sdram_cfg {
    /* Chip timing, see stm32_sdram_timing_* presets */
    const struct stm32_sdram_timing *timing;
    /* Number of column address bits 8..11 */
    uint8_t col_bits;
    /* Number of row address bits 11..13 */
    uint8_t row_bits;
    /* Number of internal banks 2 or 4 */
    uint8_t internal_banks;
    /* Data bus width as wired on the board 8, 16 or 32 */
    uint8_t bus_width;
    /* CAS latency 1..3 */
    uint8_t cas_latency;
    /* SDCLK = FMC kernel clock / sdclk_div, 2 or 3 */
    uint8_t sdclk_div;
    /* Additional read pipe delay in FMC clock cycles 0..2 */
    uint8_t read_pipe_delay;
    /* Enable read burst */
    uint8_t read_burst;
    /* FMC pins MCU_AFIO_GPIO(pin, GPIO_AFx_FMC), pin_cnt entries */
    const uint32_t *pins;
    uint8_t pin_cnt;
};

/* BSP provides configuration */
extern const struct stm32_sdram_cfg os_bsp_sdram_cfg;

/* Chip timing presets */
extern const struct stm32_sdram_timing stm32_sdram_timing_w9825g6kh_6;
extern const struct stm32_sdram_timing stm32_sdram_timing_mt48lc4m32b2_6a;

/**
 * Initialize FMC SDRAM controller and SDRAM chip.
 *
 * Called from sysinit (STM32_SDRAM_SYSINIT_STAGE).
 */
void stm32_sdram_init(void);

/**
 * @return 1 when SDRAM was initialized, 0 otherwise.
 */
int stm32_sdram_ready(void);

/**
 * @return SDRAM base address.
 */
static inline void *
stm32_sdram_base(void)
{
    return (void *)STM32_SDRAM_BASE;
}

/**
 * @return SDRAM size computed from configured geometry.
 */
size_t stm32_sdram_size(void);

/**
 * @return SDCLK frequency in Hz (valid after init).
 */
uint32_t stm32_sdram_clock(void);

/**
 * Destructive memory test of SDRAM range.
 *
 * Tests data bus, address bus and every word in range.
 * Content of tested range is lost.
 *
 * @param start   start address (inside SDRAM, 4 byte aligned)
 * @param size    number of bytes to test (multiple of 4)
 * @param fail_addr  address of first failing word (can be NULL)
 *
 * @return 0 on success, -1 on failure.
 */
int stm32_sdram_test(uint32_t start, size_t size, uint32_t *fail_addr);

#ifdef __cplusplus
}
#endif

#endif /* __STM32_SDRAM_H__ */
