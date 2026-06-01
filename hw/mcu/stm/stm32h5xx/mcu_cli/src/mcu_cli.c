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

#include "os/mynewt.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stm32h5xx_hal.h>
#include <parse/parse.h>
#include <sys/types.h>

#include "stm32h5xx_ll_rcc.h"
#include "shell/shell.h"
#include "console/console.h"

extern uint32_t SystemCoreClock;

static const char *system_clock_source[4] = {"HSI", "CSI", "HSE", "PLL1_P"};
static const char *pll_source[4] = {"---", "HSI", "CSI", "HSE"};

static const char *
on_off_state(uint32_t on)
{
    return on ? "on" : "off";
}

char *freq_str(uint32_t freq, char buf[])
{
    int freq_m = (int)(freq / 1000000);
    int freq_m_rem = (int)(freq % 1000000);
    int freq_k = (int)(freq / 1000);
    int freq_k_rem = (int)(freq % 1000);

    if (freq == 0) {
        strcpy(buf, "---");
    } else if (freq_m && freq_m_rem == 0) {
        sprintf(buf, "%d MHz", freq_m);
    } else if (freq_m) {
        while (freq_m_rem % 10 == 0) {
            freq_m_rem /= 10;
        }
        sprintf(buf, "%d.%d MHz", freq_m, freq_m_rem);
    } else if (freq_k && freq_k_rem == 0) {
        sprintf(buf, "%d kHz", freq_k);
    } else {
        sprintf(buf, "%d Hz", (int)freq);
    }
    return buf;
}

static void
print_ahb1_peripherals(struct streamer *streamer, bool all)
{
    char buf[20];

    streamer_printf(streamer, "  AHB HCLK: %s\n", freq_str(HAL_RCC_GetHCLKFreq(), buf));

    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPDMA1EN) {
        streamer_printf(streamer, "    GPDMA1  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPDMA1EN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPDMA2EN) {
        streamer_printf(streamer, "    GPDMA2  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPDMA2EN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_FLITFEN) {
        streamer_printf(streamer, "    FLITF   %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_FLITFEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_CRCEN) {
        streamer_printf(streamer, "    CRC     %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_CRCEN));
    }
#ifdef RCC_AHB1ENR_CORDICEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_CORDICEN) {
        streamer_printf(streamer, "    CORDIC  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_CORDICEN));
    }
#endif
#ifdef RCC_AHB1ENR_FMACEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_FMACEN) {
        streamer_printf(streamer, "    FMAC    %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_FMACEN));
    }
#endif
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_RAMCFGEN) {
        streamer_printf(streamer, "    RAMCFG  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_RAMCFGEN));
    }
#ifdef RCC_AHB1ENR_ETHEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_ETHEN) {
        streamer_printf(streamer, "    ETH     %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_ETHEN));
    }
#endif
#ifdef RCC_AHB1ENR_ETHTXEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_ETHTXEN) {
        streamer_printf(streamer, "    ETHTX   %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_ETHTXEN));
    }
#endif
#ifdef RCC_AHB1ENR_ETHRXEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_ETHRXEN) {
        streamer_printf(streamer, "    ETHRX   %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_ETHRXEN));
    }
#endif
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_TZSC1EN) {
        streamer_printf(streamer, "    TZSC1   %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_TZSC1EN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_BKPRAMEN) {
        streamer_printf(streamer, "    BKPRAM  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_BKPRAMEN));
    }
#ifdef RCC_AHB1ENR_DCACHE1EN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_DCACHE1EN) {
        streamer_printf(streamer, "    DCACHE1 %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_DCACHE1EN));
    }
#endif
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_SRAM1EN) {
        streamer_printf(streamer, "    SRAM1   %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_SRAM1EN));
    }
}

static const uint16_t adc_prescs[] = { 1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256 };

static void
print_ahb2_peripherals(struct streamer *streamer, bool all)
{
    char buf[20];

    if (all || RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN) {
        streamer_printf(streamer, "    GPIOA   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_GPIOAEN));
    }
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_GPIOBEN) {
        streamer_printf(streamer, "    GPIOB   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_GPIOBEN));
    }
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_GPIOCEN) {
        streamer_printf(streamer, "    GPIOC   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_GPIOCEN));
    }
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_GPIODEN) {
        streamer_printf(streamer, "    GPIOD   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_GPIODEN));
    }
#ifdef RCC_AHB2ENR_GPIOEEN
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_GPIOEEN) {
        streamer_printf(streamer, "    GPIOE   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_GPIOEEN));
    }
#endif
#ifdef RCC_AHB2ENR_GPIOFEN
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_GPIOFEN) {
        streamer_printf(streamer, "    GPIOF   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_GPIOFEN));
    }
#endif
#ifdef RCC_AHB2ENR_GPIOGEN
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_GPIOGEN) {
        streamer_printf(streamer, "    GPIOG   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_GPIOGEN));
    }
#endif
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_GPIOHEN) {
        streamer_printf(streamer, "    GPIOH   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_GPIOHEN));
    }
#if defined(RCC_AHB2ENR_GPIOIEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_GPIOIEN) {
        streamer_printf(streamer, "    GPIOI   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_GPIOIEN));
    }
#endif
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_ADCEN) {
        int prsc = (__LL_ADC_COMMON_INSTANCE(ADC1)->CCR & ADC_CCR_PRESC) >> ADC_CCR_PRESC_Pos;
        streamer_printf(streamer, "    ADC     %s %s\n",
                        on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_ADCEN),
                        freq_str(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_ADC) / adc_prescs[prsc], buf));
    }
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_DAC1EN) {
        streamer_printf(streamer, "    DAC1    %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_DAC1EN));
    }
#if defined(RCC_AHB2ENR_DCMI_PSSIEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_DCMI_PSSIEN) {
        streamer_printf(streamer, "    DCMI    %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_DCMI_PSSIEN));
    }
#endif
#if defined(RCC_AHB2ENR_AESEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_AESEN) {
        streamer_printf(streamer, "    AES     %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_AESEN));
    }
#endif
#if defined(RCC_AHB2ENR_HASHEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_HASHEN) {
        streamer_printf(streamer, "    HASH    %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_HASHEN));
    }
#endif
#if defined(RCC_AHB2ENR_RNGEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_RNGEN) {
        streamer_printf(streamer, "    RNG     %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_RNGEN));
    }
#endif
#if defined(RCC_AHB2ENR_PKAEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_PKAEN) {
        streamer_printf(streamer, "    PKA     %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_PKAEN));
    }
#endif
#if defined(RCC_AHB2ENR_SAESEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_SAESEN) {
        streamer_printf(streamer, "    SAES    %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_SAESEN));
    }
#endif
#if defined(RCC_AHB2ENR_SRAM2EN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_SRAM2EN) {
        streamer_printf(streamer, "    SRAM2   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_SRAM2EN));
    }
#endif
#if defined(RCC_AHB2ENR_SRAM3EN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_SRAM3EN) {
        streamer_printf(streamer, "    SRAM3   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_SRAM3EN));
    }
#endif
}

uint32_t sdmmc_sck_freq(uint32_t f, SDMMC_TypeDef *sdmmc)
{
    uint32_t div = ((sdmmc->CLKCR & SDMMC_CLKCR_CLKDIV_Msk) >> SDMMC_CLKCR_CLKDIV_Pos) * 2;
    if (div == 0) div = 1;
    return f / div;
}

static void
print_ahb4_peripherals(struct streamer *streamer, bool all)
{
    char buf[20];
    char buf1[20];
    static const char bus_width[4] = "148?";
    uint32_t freq;

#if defined(RCC_AHB4ENR_OTFDEC1EN)
    if (all || RCC->AHB4ENR & RCC_AHB4ENR_OTFDEC1EN) {
        streamer_printf(streamer, "    OTFDEC1 %s\n", on_off_state(RCC->AHB4ENR & RCC_AHB4ENR_OTFDEC1EN));
    }
#endif
#if defined(RCC_AHB4ENR_SDMMC1EN)
    if (all || RCC->AHB4ENR & RCC_AHB4ENR_SDMMC1EN) {
        freq = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC1);
        streamer_printf(streamer, "    SDMMC1  %s %s SD_CK %s BUS %cb\n",
                        on_off_state(RCC->AHB4ENR & RCC_AHB4ENR_SDMMC1EN),
                        freq_str(freq, buf), freq_str(sdmmc_sck_freq(freq, SDMMC1), buf1),
                        bus_width[(SDMMC1->CLKCR & SDMMC_CLKCR_WIDBUS_Msk) >> SDMMC_CLKCR_WIDBUS_Pos]);
    }
#endif
#if defined(RCC_AHB4ENR_SDMMC2EN)
    if (all || RCC->AHB4ENR & RCC_AHB4ENR_SDMMC2EN) {
        freq = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SDMMC2);
        streamer_printf(streamer, "    SDMMC2  %s %s SD_CK %s BUS %cb\n",
                        on_off_state(RCC->AHB4ENR & RCC_AHB4ENR_SDMMC2EN),
                        freq_str(freq, buf), freq_str(sdmmc_sck_freq(freq, SDMMC2), buf1),
                        bus_width[(SDMMC2->CLKCR & SDMMC_CLKCR_WIDBUS_Msk) >> SDMMC_CLKCR_WIDBUS_Pos]);
    }
#endif
#if defined(RCC_AHB4ENR_FMCEN)
    if (all || RCC->AHB4ENR & RCC_AHB4ENR_FMCEN) {
        streamer_printf(streamer, "    FMC     %s\n", on_off_state(RCC->AHB4ENR & RCC_AHB4ENR_FMCEN));
    }
#endif
#if defined(RCC_AHB4ENR_OCTOSPI1EN)
    if (all || RCC->AHB4ENR & RCC_AHB4ENR_OCTOSPI1EN) {
        streamer_printf(streamer, "    OCTOSPI1 %s\n", on_off_state(RCC->AHB4ENR & RCC_AHB4ENR_OCTOSPI1EN));
    }
#endif
}

static uint32_t
i2c_freq(uint32_t i2c_clock, uint32_t timingr)
{
    return i2c_clock / ((1 + ((timingr & I2C_TIMINGR_PRESC) >> I2C_TIMINGR_PRESC_Pos))) /
        ((((timingr & I2C_TIMINGR_SCLL) >> I2C_TIMINGR_SCLL_Pos) + 1 + 4 +
                              ((timingr & I2C_TIMINGR_SCLH) >> I2C_TIMINGR_SCLH_Pos) + 1));
}

static uint32_t
spi_freq(uint64_t periph_clk, uint32_t cfg1)
{
    uint32_t source_clock = HAL_RCCEx_GetPeriphCLKFreq(periph_clk);

    return source_clock >> (1 + ((cfg1 & SPI_CFG1_MBR_Msk) >> SPI_CFG1_MBR_Pos));
}

void stream_tim(struct streamer *streamer, TIM_TypeDef *tim, const char *name, uint32_t base_freq, bool on)
{
    char buf1[20];
    char buf2[20];
    streamer_printf(streamer, "    %-5s   %s %s (ARR %u) (UP %s)\n", name, on_off_state(on),
                    freq_str(base_freq / (tim->PSC + 1), buf1), (tim->ARR + 1),
                    ((tim->ARR + 1) != 0) ? freq_str(base_freq / (tim->PSC + 1) / (tim->ARR + 1), buf2) : "---");
}

static void
print_apb1_peripherals(struct streamer *streamer, bool all)
{
    char buf[20];
    uint32_t pckl1 = HAL_RCC_GetPCLK1Freq();
    uint32_t timmul = RCC->CFGR2 & RCC_CFGR2_PPRE1_2 ? 2 : 1;
    streamer_printf(streamer, "  APB1 PCLK1: %s\n", freq_str(pckl1, buf));
    if (all || RCC->APB1LENR & RCC_APB1LENR_TIM2EN) {
        stream_tim(streamer, TIM2, "TIM2", pckl1 * timmul, RCC->APB1LENR & RCC_APB1LENR_TIM2EN);
    }
    if (all || RCC->APB1LENR & RCC_APB1LENR_TIM3EN) {
        stream_tim(streamer, TIM3, "TIM3", pckl1 * timmul, RCC->APB1LENR & RCC_APB1LENR_TIM3EN);
    }
#ifdef RCC_APB1LENR_TIM4EN
    if (all || RCC->APB1LENR & RCC_APB1LENR_TIM4EN) {
        stream_tim(streamer, TIM4, "TIM4", pckl1 * timmul, RCC->APB1LENR & RCC_APB1LENR_TIM4EN);
    }
#endif
#ifdef RCC_APB1LENR_TIM5EN
    if (all || RCC->APB1LENR & RCC_APB1LENR_TIM5EN) {
        stream_tim(streamer, TIM5, "TIM5", pckl1 * timmul, RCC->APB1LENR & RCC_APB1LENR_TIM5EN);
    }
#endif
    if (all || RCC->APB1LENR & RCC_APB1LENR_TIM6EN) {
        stream_tim(streamer, TIM6, "TIM6", pckl1 * timmul, RCC->APB1LENR & RCC_APB1LENR_TIM6EN);
    }
    if (all || RCC->APB1LENR & RCC_APB1LENR_TIM7EN) {
        stream_tim(streamer, TIM7, "TIM7", pckl1 * timmul, RCC->APB1LENR & RCC_APB1LENR_TIM7EN);
    }
#ifdef RCC_APB1LENR_TIM12EN
    if (all || RCC->APB1LENR & RCC_APB1LENR_TIM12EN) {
        stream_tim(streamer, TIM12, "TIM12", pckl1 * timmul, RCC->APB1LENR & RCC_APB1LENR_TIM12EN);
    }
#endif
#ifdef RCC_APB1LENR_TIM13EN
    if (all || RCC->APB1LENR & RCC_APB1LENR_TIM13EN) {
        stream_tim(streamer, TIM13, "TIM13", pckl1 * timmul, RCC->APB1LENR & RCC_APB1LENR_TIM13EN);
    }
#endif
#ifdef RCC_APB1LENR_TIM14EN
    if (all || RCC->APB1LENR & RCC_APB1LENR_TIM14EN) {
        stream_tim(streamer, TIM14, "TIM14", pckl1 * timmul, RCC->APB1LENR & RCC_APB1LENR_TIM14EN);
    }
#endif
    if (all || RCC->APB1LENR & RCC_APB1LENR_WWDGEN) {
        streamer_printf(streamer, "    WWD     %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_WWDGEN));
    }
    if (all || RCC->APB1LENR & RCC_APB1LENR_SPI2EN) {
        streamer_printf(streamer, "    SPI2    %s %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_SPI2EN),
                        freq_str(spi_freq(RCC_PERIPHCLK_SPI2, SPI2->CFG1), buf));
    }
    if (all || RCC->APB1LENR & RCC_APB1LENR_SPI3EN) {
        streamer_printf(streamer, "    SPI3    %s %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_SPI3EN),
                        freq_str(spi_freq(RCC_PERIPHCLK_SPI3, SPI3->CFG1), buf));
    }
    if (all || RCC->APB1LENR & RCC_APB1LENR_USART2EN) {
        streamer_printf(streamer, "    USART2  %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_USART2EN));
    }
    if (all || RCC->APB1LENR & RCC_APB1LENR_USART3EN) {
        streamer_printf(streamer, "    USART3  %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_USART3EN));
    }
#if defined(RCC_APB1LENR_USART4EN)
    if (all || RCC->APB1LENR & RCC_APB1LENR_USART4EN) {
        streamer_printf(streamer, "    USART4  %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_USART4EN));
    }
#endif
#if defined(RCC_APB1LENR_USART5EN)
    if (all || RCC->APB1LENR & RCC_APB1LENR_USART5EN) {
        streamer_printf(streamer, "    USART5  %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_USART5EN));
    }
#endif
    if (all || RCC->APB1LENR & RCC_APB1LENR_I2C1EN) {
        streamer_printf(streamer, "    I2C1    %s %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_I2C1EN),
                        freq_str(i2c_freq(pckl1, I2C1->TIMINGR), buf));
    }
    if (all || RCC->APB1LENR & RCC_APB1LENR_I2C2EN) {
        streamer_printf(streamer, "    I2C2    %s %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_I2C2EN),
                        freq_str(i2c_freq(pckl1, I2C2->TIMINGR), buf));
    }
    if (all || RCC->APB1LENR & RCC_APB1LENR_I3C1EN) {
        streamer_printf(streamer, "    I3C1    %s %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_I2C1EN),
                        freq_str(i2c_freq(pckl1, I3C1->TIMINGR0), buf));
    }
#if defined(RCC_APB1LENR_CRSEN)
    if (all || RCC->APB1LENR & RCC_APB1LENR_CRSEN) {
        streamer_printf(streamer, "    CRS     %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_CRSEN));
    }
#endif
#if defined(RCC_APB1LENR_USART6EN)
    if (all || RCC->APB1LENR & RCC_APB1LENR_USART6EN) {
        streamer_printf(streamer, "    USART6  %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_USART6EN));
    }
#endif
#if defined(RCC_APB1LENR_USART10EN)
    if (all || RCC->APB1LENR & RCC_APB1LENR_USART10EN) {
        streamer_printf(streamer, "    USART10 %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_USART10EN));
    }
#endif
#if defined(RCC_APB1LENR_USART11EN)
    if (all || RCC->APB1LENR & RCC_APB1LENR_USART11EN) {
        streamer_printf(streamer, "    USART11 %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_USART11EN));
    }
#endif
#if defined(RCC_APB1LENR_CECEN)
    if (all || RCC->APB1LENR & RCC_APB1LENR_CECEN) {
        streamer_printf(streamer, "    CEC     %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_CECEN));
    }
#endif
#if defined(RCC_APB1LENR_USART7EN)
    if (all || RCC->APB1LENR & RCC_APB1LENR_USART7EN) {
        streamer_printf(streamer, "    USART7  %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_USART7EN));
    }
#endif
#if defined(RCC_APB1LENR_USART8EN)
    if (all || RCC->APB1LENR & RCC_APB1LENR_USART8EN) {
        streamer_printf(streamer, "    USART8  %s\n", on_off_state(RCC->APB1LENR & RCC_APB1LENR_USART8EN));
    }
#endif
#if defined(RCC_APB1HENR_UART9EN)
    if (all || RCC->APB1HENR & RCC_APB1HENR_UART9EN) {
        streamer_printf(streamer, "    UART9   %s\n", on_off_state(RCC->APB1HENR & RCC_APB1HENR_UART9EN));
    }
#endif
#if defined(RCC_APB1HENR_UART12EN)
    if (all || RCC->APB1HENR & RCC_APB1HENR_UART12EN) {
        streamer_printf(streamer, "    UART12  %s\n", on_off_state(RCC->APB1HENR & RCC_APB1HENR_UART12EN));
    }
#endif
#if defined(RCC_APB1HENR_DTSEN)
    if (all || RCC->APB1HENR & RCC_APB1HENR_DTSEN) {
        streamer_printf(streamer, "    DTS     %s\n", on_off_state(RCC->APB1HENR & RCC_APB1HENR_DTSEN));
    }
#endif
#if defined(RCC_APB1HENR_LPTIM2EN)
    if (all || RCC->APB1HENR & RCC_APB1HENR_LPTIM2EN) {
        streamer_printf(streamer, "    LPTIM2  %s\n", on_off_state(RCC->APB1HENR & RCC_APB1HENR_LPTIM2EN));
    }
#endif
#if defined(RCC_APB1HENR_FDCANEN)
    if (all || RCC->APB1HENR & RCC_APB1HENR_FDCANEN) {
        streamer_printf(streamer, "    FDCAN   %s\n", on_off_state(RCC->APB1HENR & RCC_APB1HENR_FDCANEN));
    }
#endif
#if defined(RCC_APB1HENR_UCPD1EN)
    if (all || RCC->APB1HENR & RCC_APB1HENR_UCPD1EN) {
        streamer_printf(streamer, "    UCPD1   %s\n", on_off_state(RCC->APB1HENR & RCC_APB1HENR_UCPD1EN));
    }
#endif
}

static const char *sai_clk_src[] = {"pll1_q_ck", "pll2_p_ck", "pll3_p_ck", "AUDIOCLK", "per_ck"};
static const char *usb_clk_src[] = {"---", "pll1_q_ck", "pll3_q_ck", "hsi48_ker_ck"};

static void
print_apb2_peripherals(struct streamer *streamer, bool all)
{
    char buf[20];
    uint32_t pckl = HAL_RCC_GetPCLK2Freq();
    uint32_t timmul = RCC->CFGR2 & RCC_CFGR2_PPRE2_2 ? 2 : 1;

    streamer_printf(streamer, "  APB2 PCLK2: %s\n", freq_str(pckl, buf));
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM1EN) {
        stream_tim(streamer, TIM1, "TIM1", pckl * timmul, RCC->APB2ENR & RCC_APB2ENR_TIM1EN);
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_SPI1EN) {
        streamer_printf(streamer, "    SPI1    %s %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_SPI1EN),
                        freq_str(spi_freq(RCC_PERIPHCLK_SPI1, SPI1->CFG1), buf));
    }
#ifdef RCC_APB2ENR_TIM8EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM8EN) {
        stream_tim(streamer, TIM1, "TIM8", pckl * timmul, RCC->APB2ENR & RCC_APB2ENR_TIM8EN);
    }
#endif
    if (all || RCC->APB2ENR & RCC_APB2ENR_USART1EN) {
        streamer_printf(streamer, "    USART1  %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_USART1EN));
    }
#ifdef RCC_APB2ENR_TIM15EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM15EN) {
        stream_tim(streamer, TIM15, "TIM15", pckl * timmul, RCC->APB2ENR & RCC_APB2ENR_TIM15EN);
    }
#endif
#ifdef RCC_APB2ENR_TIM16EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM16EN) {
        stream_tim(streamer, TIM16, "TIM16", pckl * timmul, RCC->APB2ENR & RCC_APB2ENR_TIM16EN);
    }
#endif
#ifdef RCC_APB2ENR_TIM17EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM17EN) {
        stream_tim(streamer, TIM17, "TIM17", pckl * timmul, RCC->APB2ENR & RCC_APB2ENR_TIM17EN);
    }
#endif
#ifdef RCC_APB2ENR_SPI4EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_SPI4EN) {
        streamer_printf(streamer, "    SPI4    %s %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_SPI4EN),
                        freq_str(spi_freq(RCC_PERIPHCLK_SPI2, SPI2->CFG1), buf));
    }
#endif
#ifdef RCC_APB2ENR_SPI6EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_SPI6EN) {
        streamer_printf(streamer, "    SPI6    %s %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_SPI6EN),
                        freq_str(spi_freq(RCC_PERIPHCLK_SPI6, SPI6->CFG1), buf));
    }
#endif
#ifdef RCC_APB2ENR_SAI1EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_SAI1EN) {
        streamer_printf(streamer, "    SAI1    %s %s (%s)\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_SAI1EN),
                        freq_str(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1), buf),
                        sai_clk_src[(RCC->CCIPR5 >> RCC_CCIPR5_SAI1SEL_Pos) & 7]);
    }
#endif
#ifdef RCC_APB2ENR_SAI2EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_SAI2EN) {
        streamer_printf(streamer, "    SAI2    %s %s (%s)\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_SAI2EN),
                        freq_str(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI2), buf),
                        sai_clk_src[(RCC->CCIPR5 >> RCC_CCIPR5_SAI2SEL_Pos) & 7]);
    }
#endif
    if (all || RCC->APB2ENR & RCC_APB2ENR_USBEN) {
        streamer_printf(streamer, "    USB     %s (%s)\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_USBEN),
                        usb_clk_src[(RCC->CCIPR4 >> RCC_CCIPR4_USBSEL_Pos) & 3]);
    }
}

static void
print_apb3_peripherals(struct streamer *streamer, bool all)
{
    char buf[20];

    streamer_printf(streamer, "  APB2 PCLK3: %s\n", freq_str(HAL_RCC_GetPCLK3Freq(), buf));
    if (all || RCC->APB3ENR & RCC_APB3ENR_SBSEN) {
        streamer_printf(streamer, "    SBS     %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_SBSEN));
    }
#ifdef RCC_APB3ENR_SPI5EN
    if (all || RCC->APB3ENR & RCC_APB3ENR_SPI5EN) {
        streamer_printf(streamer, "    SPI5    %s %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_SPI5EN),
                        freq_str(spi_freq(RCC_PERIPHCLK_SPI5, SPI5->CFG1), buf));
    }
#endif
    if (all || RCC->APB3ENR & RCC_APB3ENR_LPUART1EN) {
        streamer_printf(streamer, "    LPUART1 %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_LPUART1EN));
    }
#ifdef RCC_APB3ENR_I2C3EN
    if (all || RCC->APB3ENR & RCC_APB3ENR_I2C3EN) {
        streamer_printf(streamer, "    I2C3    %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_I2C3EN));
    }
#endif
#ifdef RCC_APB3ENR_I2C4EN
    if (all || RCC->APB3ENR & RCC_APB3ENR_I2C4EN) {
        streamer_printf(streamer, "    I2C4    %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_I2C4EN));
    }
#endif
    if (all || RCC->APB3ENR & RCC_APB3ENR_LPTIM1EN) {
        streamer_printf(streamer, "    LPTIM1  %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_LPTIM1EN));
    }
#ifdef RCC_APB3ENR_LPTIM3EN
    if (all || RCC->APB3ENR & RCC_APB3ENR_LPTIM3EN) {
        streamer_printf(streamer, "    LPTIM3  %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_LPTIM3EN));
    }
#endif
#ifdef RCC_APB3ENR_LPTIM4EN
    if (all || RCC->APB3ENR & RCC_APB3ENR_LPTIM4EN) {
        streamer_printf(streamer, "    LPTIM4  %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_LPTIM4EN));
    }
#endif
#ifdef RCC_APB3ENR_LPTIM5EN
    if (all || RCC->APB3ENR & RCC_APB3ENR_LPTIM5EN) {
        streamer_printf(streamer, "    LPTIM5  %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_LPTIM5EN));
    }
#endif
#ifdef RCC_APB3ENR_LPTIM6EN
    if (all || RCC->APB3ENR & RCC_APB3ENR_LPTIM6EN) {
        streamer_printf(streamer, "    LPTIM6  %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_LPTIM6EN));
    }
#endif
#ifdef RCC_APB3ENR_VREFEN
    if (all || RCC->APB3ENR & RCC_APB3ENR_VREFEN) {
        streamer_printf(streamer, "    VREF    %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_VREFEN));
    }
#endif
    if (all || RCC->APB3ENR & RCC_APB3ENR_RTCAPBEN) {
        streamer_printf(streamer, "    RTCAPB  %s\n", on_off_state(RCC->APB3ENR & RCC_APB3ENR_RTCAPBEN));
    }
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_param mcu_cli_info_params[] = {
    {"all", "show also disabled peripherals."},
    {NULL, NULL}
};

static const struct shell_cmd_help mcu_cli_info_help = {
    .summary = "show mcu info",
    .usage = "\n"
             "info\n"
             "  Shows clocks, and enabled peripherals.\n"
             "info all\n"
             "  Shows clocks and all peripherals.\n",
    .params = mcu_cli_info_params,
};
#endif

static int
mcu_cli_info_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                 struct streamer *streamer)
{
    char buf[20];
    bool all;
    int sw = ((RCC->CFGR1 & RCC_CFGR1_SWS) >> RCC_CFGR1_SWS_Pos);

    all = argc > 1 && strcmp(argv[1], "all") == 0;

    streamer_printf(streamer, "Clocks:\n");
    if (all || RCC->CR & RCC_CR_HSION) {
        const int div = (int)(LL_RCC_HSI_GetDivider() >> RCC_CR_HSIDIV_Pos);
        streamer_printf(streamer, "  HSI: %s %s (div: %d)\n", on_off_state(RCC->CR & RCC_CR_HSION),
            freq_str(64000000 >> div, buf), 1 << div);
    }
    if (all || RCC->CR & RCC_CR_CSION) {
        streamer_printf(streamer, "  CSI: %s\n", on_off_state(RCC->CR & RCC_CR_CSION));
    }
    if (all || RCC->CR & RCC_CR_HSI48ON) {
        streamer_printf(streamer, "  HSI48: %s\n", on_off_state(RCC->CR & RCC_CR_HSI48ON));
    }
    if (all || RCC->CR & RCC_CR_HSEON) {
        streamer_printf(streamer, "  HSE: %s %s\n", on_off_state(RCC->CR & RCC_CR_HSEON),
            freq_str(HSE_VALUE, buf));
    }
    if (all || RCC->CR & RCC_CR_PLL1ON) {
        PLL1_ClocksTypeDef clocks;
        HAL_RCCEx_GetPLL1ClockFreq(&clocks);
        streamer_printf(streamer, "  PLL1: %s src: %s\n", on_off_state(RCC->CR & RCC_CR_PLL1ON),
                        pll_source[RCC->PLL1CFGR & RCC_PLL1CFGR_PLL1SRC]);
        streamer_printf(streamer, "    PLL1P: %s\n", freq_str(clocks.PLL1_P_Frequency, buf));
        streamer_printf(streamer, "    PLL1Q: %s\n", freq_str(clocks.PLL1_Q_Frequency, buf));
        streamer_printf(streamer, "    PLL1R: %s\n", freq_str(clocks.PLL1_R_Frequency, buf));
    }
    if (all || RCC->CR & RCC_CR_PLL2ON) {
        PLL2_ClocksTypeDef clocks;
        HAL_RCCEx_GetPLL2ClockFreq(&clocks);
        streamer_printf(streamer, "  PLL2: %s src: %s\n", on_off_state(RCC->CR & RCC_CR_PLL2ON),
                        pll_source[RCC->PLL2CFGR & RCC_PLL2CFGR_PLL2SRC]);
        streamer_printf(streamer, "    PLL2P: %s\n", freq_str(clocks.PLL2_P_Frequency, buf));
        streamer_printf(streamer, "    PLL2Q: %s\n", freq_str(clocks.PLL2_Q_Frequency, buf));
        streamer_printf(streamer, "    PLL2R: %s\n", freq_str(clocks.PLL2_R_Frequency, buf));
    }
#ifdef RCC_CR_PLL3ON
    if (all || RCC->CR & RCC_CR_PLL3ON) {
        streamer_printf(streamer, "  PLL3: %s src: %s\n", on_off_state(RCC->CR & RCC_CR_PLL3ON),
                        pll_source[RCC->PLL3CFGR & RCC_PLL3CFGR_PLL3SRC]);
    }
#endif
    if (all || RCC->BDCR & RCC_BDCR_LSION) {
        streamer_printf(streamer, "  LSI: %s\n", on_off_state(RCC->BDCR & RCC_BDCR_LSION));
    }
    if (all || RCC->BDCR & RCC_BDCR_LSEON) {
        streamer_printf(streamer, "  LSE: %s\n", on_off_state(RCC->BDCR & RCC_BDCR_LSEON));
    }
    if (all || RCC->BDCR & RCC_BDCR_RTCEN) {
        streamer_printf(streamer, "  RTC: %s\n", on_off_state(RCC->BDCR & RCC_BDCR_RTCEN));
    }
    streamer_printf(streamer, "  SYSCLK: %s\n", freq_str(SystemCoreClock, buf));
    streamer_printf(streamer, "    source: %s\n", system_clock_source[sw]);
    streamer_printf(streamer, "Peripherals:\n");
    print_ahb1_peripherals(streamer, all);
    print_ahb2_peripherals(streamer, all);
    print_ahb4_peripherals(streamer, all);
    print_apb1_peripherals(streamer, all);
    print_apb2_peripherals(streamer, all);
    print_apb3_peripherals(streamer, all);

    return 0;
}

static const struct shell_cmd mcu_cli_commands[] = {
    SHELL_CMD_EXT("info", mcu_cli_info_cmd, &mcu_cli_info_help),
};

SHELL_MODULE_WITH_TABLE(mcu, mcu_cli_commands);
