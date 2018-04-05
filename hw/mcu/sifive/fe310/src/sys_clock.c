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

#include <stdlib.h>
#include "os/mynewt.h"
#include <env/freedom-e300-hifive1/platform.h>
#include <env/encoding.h>
#include <mcu/plic.h>
#include <mcu/sys_clock.h>

#define PLL_DIVR(r) ((r) - 1)
#define PLL_MULF(f) ((f) / 2 - 1)
#define LOG2_2  1
#define LOG2_4  2
#define LOG2_8  3
#define PLL_DIVQ(q) LOG2_##q

/*
 * Following set of variables can be used to select system clock.
 * Unreferenced variables can be removed by linker.
 * For HFROSC_xxxxx variable accurate frequency is not know, it depends
 * on oscillator. Provided value is used to correctly select QSPI
 * divider so clock used for flash does not exceed correct value
 * If serial port is used it's better to use HFXOSC_ instead of
 * untrimmed internal oscillator.
 * For clocks lower then 16MHz (even when HFXOSC is used) UART operate
 * correctly for baudrate 125000 not 115200.
 */
const clock_config_t
HFROSC             = { 82000000, 0, 0,  0, 0, 0, 0, 1, 0}; /* HFROSC */
const clock_config_t
HFROSC_DIV_2       = { 41000000, 0, 0,  1, 0, 0, 0, 1, 0}; /* HFROSC / 2 */
const clock_config_t
HFROSC_DIV_3       = { 27300000, 0, 0,  2, 0, 0, 0, 1, 0}; /* HFROSC / 3 */
const clock_config_t
HFROSC_DIV_4       = { 21000000, 0, 0,  3, 0, 0, 0, 1, 0}; /* HFROSC / 4 */
const clock_config_t
HFROSC_DIV_6       = { 14000000, 0, 0,  5, 0, 0, 0, 1, 0}; /* HFROSC / 6 */
const clock_config_t
HFROSC_DIV_12      = {  7000000, 0, 0, 11, 0, 0, 0, 1, 0}; /* HFROSC / 12 */
const clock_config_t
HFROSC_DIV_24      = {  4000000, 0, 0, 23, 0, 0, 0, 1, 0}; /* HFROSC / 24 */
const clock_config_t
HFROSC_DIV_36      = {  3000000, 0, 0, 35, 0, 0, 0, 1, 0}; /* HFROSC / 36 */
const clock_config_t
HFROSC_DIV_64      = {  1250000, 0, 0, 63, 0, 0, 0, 1, 0}; /* HFROSC / 64 */

const clock_config_t
HFXOSC_PLL_320_MHZ = {320000000, 1, 1,  4, PLL_DIVR(2), PLL_MULF(80), PLL_DIVQ(2), 1, 0}; /* 320 MHz */
const clock_config_t
HFXOSC_PLL_256_MHZ = {256000000, 1, 1,  4, PLL_DIVR(2), PLL_MULF(64), PLL_DIVQ(2), 1, 0}; /* 256 MHz */
const clock_config_t
HFXOSC_PLL_128_MHZ = {128000000, 1, 1,  4, PLL_DIVR(2), PLL_MULF(64), PLL_DIVQ(4), 1, 0}; /* 128 MHz */
const clock_config_t
HFXOSC_PLL_64_MHZ  = { 64000000, 1, 1,  4, PLL_DIVR(2), PLL_MULF(64), PLL_DIVQ(8), 1, 0}; /* 64 MHz */
const clock_config_t
HFXOSC_PLL_32_MHZ  = { 32000000, 1, 1,  4, PLL_DIVR(2), PLL_MULF(64), PLL_DIVQ(8), 0, 0}; /* 32 MHz */
const clock_config_t
HFXOSC_16_MHZ      = { 16000000, 1, 0,  0, 0, 0, 0, 1, 0}; /* 16 MHz */
const clock_config_t
HFXOSC_8_MHZ       = {  8000000, 1, 0,  0, 0, 0, 0, 0, 0}; /* 8 MHz */
const clock_config_t
HFXOSC_4_MHZ       = {  4000000, 1, 0,  0, 0, 0, 0, 0, 1}; /* 4 MHz */
const clock_config_t
HFXOSC_2_MHZ       = {  2000000, 1, 0,  0, 0, 0, 0, 0, 3}; /* 2 MHz */
const clock_config_t
HFXOSC_1_MHZ       = {  1000000, 1, 0,  0, 0, 0, 0, 0, 7}; /* 1 MHz */

static uint32_t cpu_freq;

static unsigned long __attribute__((noinline))
measure_cpu_freq(size_t n)
{
    unsigned long start_mtime, delta_mtime;
    unsigned long mtime_freq = get_timer_freq();

    // Don't start measuruing until we see an mtime tick
    unsigned long tmp = mtime_lo();
    do {
        start_mtime = mtime_lo();
    } while (start_mtime == tmp);

    unsigned long start_mcycle = read_csr(mcycle);

    do {
        delta_mtime = mtime_lo() - start_mtime;
    } while (delta_mtime < n);

    unsigned long delta_mcycle = read_csr(mcycle) - start_mcycle;

    return (delta_mcycle / delta_mtime) * mtime_freq + ((delta_mcycle % delta_mtime) * mtime_freq) / delta_mtime;
}

uint32_t
get_cpu_freq(void)
{

    if (!cpu_freq) {
        // warm up I$
        measure_cpu_freq(1);
        // measure for real
        cpu_freq = measure_cpu_freq(10);
    }

    return cpu_freq;
}

void
select_clock(const clock_config_t *cfg)
{
    uint32_t new_qspi_div = 0;
    uint32_t old_qspi_div = SPI0_REG(SPI_REG_SCKDIV);

    /* Clock based on external escilator */
    if (!cfg->xosc || cfg->pll) {
        /* Turn on internal oscillator */
        PRCI_REG(PRCI_HFROSCCFG) = ROSC_DIV(cfg->osc_div) |
                                   ROSC_TRIM(MYNEWT_VAL(HFROSC_DEFAULT_TRIM_VAL)) |
                                   ROSC_EN(1);
        while ((PRCI_REG(PRCI_HFROSCCFG) & ROSC_RDY(1)) == 0) {
        }
        /* Compute CPU frequency on demand */
        cpu_freq = 0;
    }

    if (cfg->xosc) {
        /* Turn on external oscillator if not ready yet */
        if ((PRCI_REG(PRCI_HFXOSCCFG) & XOSC_RDY(1)) == 0) {
            PRCI_REG(PRCI_HFXOSCCFG) = XOSC_EN(1);
            while ((PRCI_REG(PRCI_HFXOSCCFG) & XOSC_RDY(1)) == 0) {
            }
        }
        cpu_freq = cfg->frq;
    }

    /*
     * If reqested closk is higher then FLASH_MAX_CLOCK change divider so QSPI
     * clock is in rage.
     */
    if (cfg->frq >= MYNEWT_VAL(FLASH_MAX_CLOCK) * 2) {
        new_qspi_div = (cfg->frq + MYNEWT_VAL(FLASH_MAX_CLOCK) - 1) / (2 * MYNEWT_VAL(FLASH_MAX_CLOCK)) - 1;
    }

    /* New qspi divider is higher, reduce qspi clock before switching to higher clock */
    if (new_qspi_div > old_qspi_div) {
        SPI0_REG(SPI_REG_SCKDIV) = new_qspi_div;
    }

    PRCI_REG(PRCI_PLLDIV) = PLL_FINAL_DIV_BY_1(cfg->pll_outdiv1) |
                            PLL_FINAL_DIV(cfg->pll_out_div);

    if (cfg->pll) {
        uint32_t now;
        uint32_t pllcfg = PLL_REFSEL(cfg->xosc) |
                          PLL_R(cfg->pll_div_r) | PLL_F(cfg->pll_mul_f) | PLL_Q(cfg->pll_div_q);
        PRCI_REG(PRCI_PLLCFG) = PLL_BYPASS(1) | pllcfg;
        PRCI_REG(PRCI_PLLCFG) ^= PLL_BYPASS(1);

        /* 100us lock grace period */
        now = mtime_lo();
        while (mtime_lo() - now < 4) {
        }

        /* Now it is safe to check for PLL Lock */
        while ((PRCI_REG(PRCI_PLLCFG) & PLL_LOCK(1)) == 0) {
        }

        /* Select PLL as clock source */
        PRCI_REG(PRCI_PLLCFG) |= PLL_SEL(1) | pllcfg;
    } else {
        /* Select bypassed PLL as source signal, it allows to use HFXOSC */
        PRCI_REG(PRCI_PLLCFG) = PLL_BYPASS(1) | PLL_REFSEL(cfg->xosc) | PLL_SEL(1);
    }

    /*
     * Old qspi divider was higher, now it's safe to reduce divider, increasing
     * qspi clock for better performance
     */
    if (new_qspi_div < old_qspi_div) {
        SPI0_REG(SPI_REG_SCKDIV) = new_qspi_div;
    }
}
