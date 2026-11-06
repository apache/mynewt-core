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
#include <stdio.h>
#include <string.h>
#include <stm32g0xx_hal.h>
#include <parse/parse.h>
#include <shell/shell.h>

extern uint32_t SystemCoreClock;

static const char *system_clock_source[8] = {
    "HSISYS", "HSE", "PLLR", "LSI", "LSE", "?", "?", "?"
};
static const char *pll_source[4] = { "---", "?", "HSI16", "HSE" };
static const char *rtc_source[4] = { "---", "LSE", "LSI", "HSE/32" };
static const char *uart_clk_src[4] = { "PCLK", "SYSCLK", "HSI16", "LSE" };
#if defined(RCC_CCIPR2_USBSEL)
static const char *usb_clk_src[4] = { "HSI48", "HSE", "PLLQ", "?" };
#endif
/* USART/LPUART PRESC and ADC CCR PRESC encoding */
static const uint16_t prescs[16] = {
    1, 2, 4, 6, 8, 10, 12, 16, 32, 64, 128, 256, 256, 256, 256, 256
};

static const char *
on_off_state(uint32_t on)
{
    return on ? "on" : "off";
}

static char *
freq_str(uint32_t freq, char buf[])
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

/* Timer kernel clock: PCLK, doubled when APB prescaler is not 1 */
static uint32_t
tim_pclk(void)
{
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();

    return (RCC->CFGR & RCC_CFGR_PPRE_2) ? pclk * 2 : pclk;
}

/* Registers of peripheral without clock read as 0, print name only */
static bool
stream_off(struct streamer *streamer, const char *name, bool on)
{
    if (!on) {
        streamer_printf(streamer, "    %-7s off\n", name);
    }
    return !on;
}

static void
stream_tim(struct streamer *streamer, TIM_TypeDef *tim, const char *name, uint32_t base_freq, bool on)
{
    char buf1[20];
    char buf2[20];
    uint32_t cnt_freq;
    uint32_t arr;

    if (stream_off(streamer, name, on)) {
        return;
    }
    cnt_freq = base_freq / (tim->PSC + 1);
    arr = tim->ARR + 1;
    streamer_printf(streamer, "    %-7s on %s (ARR %u) (UP %s)%s\n", name,
                    freq_str(cnt_freq, buf1), (unsigned int)arr,
                    arr ? freq_str(cnt_freq / arr, buf2) : "---",
                    (tim->CR1 & TIM_CR1_CEN) ? "" : " stopped");
}

static void
stream_spi(struct streamer *streamer, SPI_TypeDef *spi, const char *name, bool on)
{
    char buf[20];

    if (stream_off(streamer, name, on)) {
        return;
    }
    streamer_printf(streamer, "    %-7s on %s%s\n", name,
                    freq_str(HAL_RCC_GetPCLK1Freq() >> (1 + ((spi->CR1 & SPI_CR1_BR) >> SPI_CR1_BR_Pos)), buf),
                    (spi->CR1 & SPI_CR1_SPE) ? "" : " disabled");
}

static uint32_t
i2c_freq(uint32_t i2c_clock, uint32_t timingr)
{
    if (timingr == 0) {
        return 0;
    }
    return i2c_clock / ((1 + ((timingr & I2C_TIMINGR_PRESC) >> I2C_TIMINGR_PRESC_Pos))) /
        ((((timingr & I2C_TIMINGR_SCLL) >> I2C_TIMINGR_SCLL_Pos) + 1 + 4 +
          ((timingr & I2C_TIMINGR_SCLH) >> I2C_TIMINGR_SCLH_Pos) + 1));
}

static void
stream_i2c(struct streamer *streamer, I2C_TypeDef *i2c, const char *name, uint32_t clk, bool on)
{
    char buf[20];

    if (stream_off(streamer, name, on)) {
        return;
    }
    streamer_printf(streamer, "    %-7s on %s (TIMINGR 0x%08X)%s\n", name,
                    freq_str(i2c_freq(clk, i2c->TIMINGR), buf), (unsigned int)i2c->TIMINGR,
                    (i2c->CR1 & I2C_CR1_PE) ? "" : " disabled");
}

static void
stream_baud(struct streamer *streamer, USART_TypeDef *usart, const char *name, uint32_t baud,
            const char *src)
{
    if (usart->CR1 & USART_CR1_UE) {
        streamer_printf(streamer, "    %-7s on %u bps (%s)\n", name, (unsigned int)baud, src);
    } else {
        streamer_printf(streamer, "    %-7s on (%s) disabled\n", name, src);
    }
}

static void
stream_usart(struct streamer *streamer, USART_TypeDef *usart, const char *name, uint32_t clk,
             const char *src, bool on)
{
    uint32_t brr;
    uint32_t baud = 0;

    if (stream_off(streamer, name, on)) {
        return;
    }
    brr = usart->BRR & 0xFFFF;
    clk /= prescs[usart->PRESC & USART_PRESC_PRESCALER];
    if (usart->CR1 & USART_CR1_OVER8) {
        brr = (brr & ~0xFU) | ((brr & 7) << 1);
        clk *= 2;
    }
    if (brr) {
        baud = clk / brr;
    }
    stream_baud(streamer, usart, name, baud, src);
}

static void
stream_lpuart(struct streamer *streamer, USART_TypeDef *lpuart, const char *name, uint32_t clk,
              const char *src, bool on)
{
    uint32_t brr;
    uint32_t baud = 0;

    if (stream_off(streamer, name, on)) {
        return;
    }
    brr = lpuart->BRR & 0xFFFFF;
    clk /= prescs[lpuart->PRESC & USART_PRESC_PRESCALER];
    if (brr) {
        baud = (uint32_t)(((uint64_t)clk * 256) / brr);
    }
    stream_baud(streamer, lpuart, name, baud, src);
}

static void
print_iop_peripherals(struct streamer *streamer, bool all)
{
    if (all || RCC->IOPENR & RCC_IOPENR_GPIOAEN) {
        streamer_printf(streamer, "    GPIOA   %s\n", on_off_state(RCC->IOPENR & RCC_IOPENR_GPIOAEN));
    }
    if (all || RCC->IOPENR & RCC_IOPENR_GPIOBEN) {
        streamer_printf(streamer, "    GPIOB   %s\n", on_off_state(RCC->IOPENR & RCC_IOPENR_GPIOBEN));
    }
    if (all || RCC->IOPENR & RCC_IOPENR_GPIOCEN) {
        streamer_printf(streamer, "    GPIOC   %s\n", on_off_state(RCC->IOPENR & RCC_IOPENR_GPIOCEN));
    }
    if (all || RCC->IOPENR & RCC_IOPENR_GPIODEN) {
        streamer_printf(streamer, "    GPIOD   %s\n", on_off_state(RCC->IOPENR & RCC_IOPENR_GPIODEN));
    }
#ifdef RCC_IOPENR_GPIOEEN
    if (all || RCC->IOPENR & RCC_IOPENR_GPIOEEN) {
        streamer_printf(streamer, "    GPIOE   %s\n", on_off_state(RCC->IOPENR & RCC_IOPENR_GPIOEEN));
    }
#endif
    if (all || RCC->IOPENR & RCC_IOPENR_GPIOFEN) {
        streamer_printf(streamer, "    GPIOF   %s\n", on_off_state(RCC->IOPENR & RCC_IOPENR_GPIOFEN));
    }
}

static void
print_ahb_peripherals(struct streamer *streamer, bool all)
{
    char buf[20];

    streamer_printf(streamer, "  AHB HCLK: %s\n", freq_str(HAL_RCC_GetHCLKFreq(), buf));
    print_iop_peripherals(streamer, all);
    if (all || RCC->AHBENR & RCC_AHBENR_DMA1EN) {
        streamer_printf(streamer, "    DMA1    %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_DMA1EN));
    }
#ifdef RCC_AHBENR_DMA2EN
    if (all || RCC->AHBENR & RCC_AHBENR_DMA2EN) {
        streamer_printf(streamer, "    DMA2    %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_DMA2EN));
    }
#endif
    if (all || RCC->AHBENR & RCC_AHBENR_FLASHEN) {
        streamer_printf(streamer, "    FLASH   %s latency %u\n", on_off_state(RCC->AHBENR & RCC_AHBENR_FLASHEN),
                        (unsigned int)((FLASH->ACR & FLASH_ACR_LATENCY) >> FLASH_ACR_LATENCY_Pos));
    }
    if (all || RCC->AHBENR & RCC_AHBENR_CRCEN) {
        streamer_printf(streamer, "    CRC     %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_CRCEN));
    }
#ifdef RCC_AHBENR_AESEN
    if (all || RCC->AHBENR & RCC_AHBENR_AESEN) {
        streamer_printf(streamer, "    AES     %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_AESEN));
    }
#endif
#ifdef RCC_AHBENR_RNGEN
    if (all || RCC->AHBENR & RCC_AHBENR_RNGEN) {
        streamer_printf(streamer, "    RNG     %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_RNGEN));
    }
#endif
}

static void
print_apb1_peripherals(struct streamer *streamer, bool all)
{
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t timclk = tim_pclk();

    if (all || RCC->APBENR1 & RCC_APBENR1_TIM2EN) {
        stream_tim(streamer, TIM2, "TIM2", timclk, RCC->APBENR1 & RCC_APBENR1_TIM2EN);
    }
    if (all || RCC->APBENR1 & RCC_APBENR1_TIM3EN) {
        stream_tim(streamer, TIM3, "TIM3", timclk, RCC->APBENR1 & RCC_APBENR1_TIM3EN);
    }
#ifdef RCC_APBENR1_TIM4EN
    if (all || RCC->APBENR1 & RCC_APBENR1_TIM4EN) {
        stream_tim(streamer, TIM4, "TIM4", timclk, RCC->APBENR1 & RCC_APBENR1_TIM4EN);
    }
#endif
#ifdef RCC_APBENR1_TIM6EN
    if (all || RCC->APBENR1 & RCC_APBENR1_TIM6EN) {
        stream_tim(streamer, TIM6, "TIM6", timclk, RCC->APBENR1 & RCC_APBENR1_TIM6EN);
    }
#endif
#ifdef RCC_APBENR1_TIM7EN
    if (all || RCC->APBENR1 & RCC_APBENR1_TIM7EN) {
        stream_tim(streamer, TIM7, "TIM7", timclk, RCC->APBENR1 & RCC_APBENR1_TIM7EN);
    }
#endif
    if (all || RCC->APBENR1 & RCC_APBENR1_RTCAPBEN) {
        streamer_printf(streamer, "    RTCAPB  %s\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_RTCAPBEN));
    }
    if (all || RCC->APBENR1 & RCC_APBENR1_WWDGEN) {
        streamer_printf(streamer, "    WWDG    %s\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_WWDGEN));
    }
#ifdef RCC_APBENR1_FDCANEN
    if (all || RCC->APBENR1 & RCC_APBENR1_FDCANEN) {
        char buf[20];
        streamer_printf(streamer, "    FDCAN   %s %s\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_FDCANEN),
                        freq_str(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_FDCAN), buf));
    }
#endif
#ifdef RCC_APBENR1_USBEN
    if (all || RCC->APBENR1 & RCC_APBENR1_USBEN) {
        char buf[20];
        streamer_printf(streamer, "    USB     %s %s (%s)\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_USBEN),
                        freq_str(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_USB), buf),
                        usb_clk_src[(RCC->CCIPR2 & RCC_CCIPR2_USBSEL) >> RCC_CCIPR2_USBSEL_Pos]);
    }
#endif
#ifdef RCC_APBENR1_SPI2EN
    if (all || RCC->APBENR1 & RCC_APBENR1_SPI2EN) {
        stream_spi(streamer, SPI2, "SPI2", RCC->APBENR1 & RCC_APBENR1_SPI2EN);
    }
#endif
#ifdef RCC_APBENR1_SPI3EN
    if (all || RCC->APBENR1 & RCC_APBENR1_SPI3EN) {
        stream_spi(streamer, SPI3, "SPI3", RCC->APBENR1 & RCC_APBENR1_SPI3EN);
    }
#endif
#ifdef RCC_APBENR1_CRSEN
    if ((all || RCC->APBENR1 & RCC_APBENR1_CRSEN) &&
        !stream_off(streamer, "CRS", RCC->APBENR1 & RCC_APBENR1_CRSEN)) {
        streamer_printf(streamer, "    CRS     on%s TRIM %u\n", (CRS->CR & CRS_CR_CEN) ? "" : " disabled",
                        (unsigned int)((CRS->CR & CRS_CR_TRIM) >> CRS_CR_TRIM_Pos));
    }
#endif
    if (all || RCC->APBENR1 & RCC_APBENR1_USART2EN) {
#ifdef RCC_CCIPR_USART2SEL
        stream_usart(streamer, USART2, "USART2", HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_USART2),
                     uart_clk_src[(RCC->CCIPR & RCC_CCIPR_USART2SEL) >> RCC_CCIPR_USART2SEL_Pos],
                     RCC->APBENR1 & RCC_APBENR1_USART2EN);
#else
        stream_usart(streamer, USART2, "USART2", pclk, "PCLK", RCC->APBENR1 & RCC_APBENR1_USART2EN);
#endif
    }
#ifdef RCC_APBENR1_USART3EN
    if (all || RCC->APBENR1 & RCC_APBENR1_USART3EN) {
#ifdef RCC_CCIPR_USART3SEL
        stream_usart(streamer, USART3, "USART3", HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_USART3),
                     uart_clk_src[(RCC->CCIPR & RCC_CCIPR_USART3SEL) >> RCC_CCIPR_USART3SEL_Pos],
                     RCC->APBENR1 & RCC_APBENR1_USART3EN);
#else
        stream_usart(streamer, USART3, "USART3", pclk, "PCLK", RCC->APBENR1 & RCC_APBENR1_USART3EN);
#endif
    }
#endif
#ifdef RCC_APBENR1_USART4EN
    if (all || RCC->APBENR1 & RCC_APBENR1_USART4EN) {
        stream_usart(streamer, USART4, "USART4", pclk, "PCLK", RCC->APBENR1 & RCC_APBENR1_USART4EN);
    }
#endif
#ifdef RCC_APBENR1_USART5EN
    if (all || RCC->APBENR1 & RCC_APBENR1_USART5EN) {
        stream_usart(streamer, USART5, "USART5", pclk, "PCLK", RCC->APBENR1 & RCC_APBENR1_USART5EN);
    }
#endif
#ifdef RCC_APBENR1_USART6EN
    if (all || RCC->APBENR1 & RCC_APBENR1_USART6EN) {
        stream_usart(streamer, USART6, "USART6", pclk, "PCLK", RCC->APBENR1 & RCC_APBENR1_USART6EN);
    }
#endif
#ifdef RCC_APBENR1_LPUART1EN
    if (all || RCC->APBENR1 & RCC_APBENR1_LPUART1EN) {
        stream_lpuart(streamer, LPUART1, "LPUART1", HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_LPUART1),
                      uart_clk_src[(RCC->CCIPR & RCC_CCIPR_LPUART1SEL) >> RCC_CCIPR_LPUART1SEL_Pos],
                      RCC->APBENR1 & RCC_APBENR1_LPUART1EN);
    }
#endif
#ifdef RCC_APBENR1_LPUART2EN
    if (all || RCC->APBENR1 & RCC_APBENR1_LPUART2EN) {
        stream_lpuart(streamer, LPUART2, "LPUART2", HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_LPUART2),
                      uart_clk_src[(RCC->CCIPR & RCC_CCIPR_LPUART2SEL) >> RCC_CCIPR_LPUART2SEL_Pos],
                      RCC->APBENR1 & RCC_APBENR1_LPUART2EN);
    }
#endif
    if (all || RCC->APBENR1 & RCC_APBENR1_I2C1EN) {
        stream_i2c(streamer, I2C1, "I2C1", HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2C1),
                   RCC->APBENR1 & RCC_APBENR1_I2C1EN);
    }
    if (all || RCC->APBENR1 & RCC_APBENR1_I2C2EN) {
#ifdef RCC_CCIPR_I2C2SEL
        stream_i2c(streamer, I2C2, "I2C2", HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_I2C2),
                   RCC->APBENR1 & RCC_APBENR1_I2C2EN);
#else
        stream_i2c(streamer, I2C2, "I2C2", pclk, RCC->APBENR1 & RCC_APBENR1_I2C2EN);
#endif
    }
#ifdef RCC_APBENR1_I2C3EN
    if (all || RCC->APBENR1 & RCC_APBENR1_I2C3EN) {
        stream_i2c(streamer, I2C3, "I2C3", pclk, RCC->APBENR1 & RCC_APBENR1_I2C3EN);
    }
#endif
#ifdef RCC_APBENR1_CECEN
    if (all || RCC->APBENR1 & RCC_APBENR1_CECEN) {
        streamer_printf(streamer, "    CEC     %s\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_CECEN));
    }
#endif
#ifdef RCC_APBENR1_UCPD1EN
    if (all || RCC->APBENR1 & RCC_APBENR1_UCPD1EN) {
        streamer_printf(streamer, "    UCPD1   %s\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_UCPD1EN));
    }
#endif
#ifdef RCC_APBENR1_UCPD2EN
    if (all || RCC->APBENR1 & RCC_APBENR1_UCPD2EN) {
        streamer_printf(streamer, "    UCPD2   %s\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_UCPD2EN));
    }
#endif
    if (all || RCC->APBENR1 & RCC_APBENR1_DBGEN) {
        streamer_printf(streamer, "    DBG     %s\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_DBGEN));
    }
    if ((all || RCC->APBENR1 & RCC_APBENR1_PWREN) &&
        !stream_off(streamer, "PWR", RCC->APBENR1 & RCC_APBENR1_PWREN)) {
        streamer_printf(streamer, "    PWR     on VOS range %u\n", (unsigned int)((PWR->CR1 & PWR_CR1_VOS) >> PWR_CR1_VOS_Pos));
    }
#ifdef RCC_APBENR1_DAC1EN
    if (all || RCC->APBENR1 & RCC_APBENR1_DAC1EN) {
        streamer_printf(streamer, "    DAC1    %s\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_DAC1EN));
    }
#endif
#ifdef RCC_APBENR1_LPTIM1EN
    if (all || RCC->APBENR1 & RCC_APBENR1_LPTIM1EN) {
        char buf[20];
        streamer_printf(streamer, "    LPTIM1  %s %s\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_LPTIM1EN),
                        freq_str(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_LPTIM1), buf));
    }
#endif
#ifdef RCC_APBENR1_LPTIM2EN
    if (all || RCC->APBENR1 & RCC_APBENR1_LPTIM2EN) {
        char buf[20];
        streamer_printf(streamer, "    LPTIM2  %s %s\n", on_off_state(RCC->APBENR1 & RCC_APBENR1_LPTIM2EN),
                        freq_str(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_LPTIM2), buf));
    }
#endif
}

static uint32_t
adc_freq(void)
{
    uint32_t ckmode = (ADC1->CFGR2 & ADC_CFGR2_CKMODE) >> ADC_CFGR2_CKMODE_Pos;
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();

    switch (ckmode) {
    case 0:
        return HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_ADC) /
               prescs[(ADC1_COMMON->CCR & ADC_CCR_PRESC) >> ADC_CCR_PRESC_Pos];
    case 1:
        return pclk / 2;
    case 2:
        return pclk / 4;
    default:
        return pclk;
    }
}

static void
print_apb2_peripherals(struct streamer *streamer, bool all)
{
    uint32_t timclk = tim_pclk();
    uint32_t tim1clk = timclk;
    uint32_t tim15clk = timclk;

#ifdef RCC_CCIPR_TIM1SEL
    if (RCC->CCIPR & RCC_CCIPR_TIM1SEL) {
        tim1clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_TIM1);
    }
#endif
#ifdef RCC_CCIPR_TIM15SEL
    if (RCC->CCIPR & RCC_CCIPR_TIM15SEL) {
        tim15clk = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_TIM15);
    }
#endif
    (void)tim15clk;

    if (all || RCC->APBENR2 & RCC_APBENR2_SYSCFGEN) {
        streamer_printf(streamer, "    SYSCFG  %s\n", on_off_state(RCC->APBENR2 & RCC_APBENR2_SYSCFGEN));
    }
    if (all || RCC->APBENR2 & RCC_APBENR2_TIM1EN) {
        stream_tim(streamer, TIM1, "TIM1", tim1clk, RCC->APBENR2 & RCC_APBENR2_TIM1EN);
    }
    if (all || RCC->APBENR2 & RCC_APBENR2_SPI1EN) {
        stream_spi(streamer, SPI1, "SPI1", RCC->APBENR2 & RCC_APBENR2_SPI1EN);
    }
    if (all || RCC->APBENR2 & RCC_APBENR2_USART1EN) {
        stream_usart(streamer, USART1, "USART1", HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_USART1),
                     uart_clk_src[(RCC->CCIPR & RCC_CCIPR_USART1SEL) >> RCC_CCIPR_USART1SEL_Pos],
                     RCC->APBENR2 & RCC_APBENR2_USART1EN);
    }
    if (all || RCC->APBENR2 & RCC_APBENR2_TIM14EN) {
        stream_tim(streamer, TIM14, "TIM14", timclk, RCC->APBENR2 & RCC_APBENR2_TIM14EN);
    }
#ifdef RCC_APBENR2_TIM15EN
    if (all || RCC->APBENR2 & RCC_APBENR2_TIM15EN) {
        stream_tim(streamer, TIM15, "TIM15", tim15clk, RCC->APBENR2 & RCC_APBENR2_TIM15EN);
    }
#endif
    if (all || RCC->APBENR2 & RCC_APBENR2_TIM16EN) {
        stream_tim(streamer, TIM16, "TIM16", timclk, RCC->APBENR2 & RCC_APBENR2_TIM16EN);
    }
    if (all || RCC->APBENR2 & RCC_APBENR2_TIM17EN) {
        stream_tim(streamer, TIM17, "TIM17", timclk, RCC->APBENR2 & RCC_APBENR2_TIM17EN);
    }
    if ((all || RCC->APBENR2 & RCC_APBENR2_ADCEN) &&
        !stream_off(streamer, "ADC", RCC->APBENR2 & RCC_APBENR2_ADCEN)) {
        char buf[20];
        streamer_printf(streamer, "    ADC     on %s\n", freq_str(adc_freq(), buf));
    }
}

static void
print_clocks(struct streamer *streamer, bool all)
{
    char buf[20];
    uint32_t pllcfgr = RCC->PLLCFGR;
    uint32_t pll_src = (pllcfgr & RCC_PLLCFGR_PLLSRC) >> RCC_PLLCFGR_PLLSRC_Pos;
    int sw = (int)((RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos);

    streamer_printf(streamer, "Clocks:\n");
    if (all || RCC->CR & RCC_CR_HSION) {
        const int div = (int)((RCC->CR & RCC_CR_HSIDIV) >> RCC_CR_HSIDIV_Pos);
        streamer_printf(streamer, "  HSI16: %s HSISYS %s (div: %d)\n", on_off_state(RCC->CR & RCC_CR_HSION),
                        freq_str(HSI_VALUE >> div, buf), 1 << div);
    }
#ifdef RCC_CR_HSI48ON
    if (all || RCC->CR & RCC_CR_HSI48ON) {
        streamer_printf(streamer, "  HSI48: %s\n", on_off_state(RCC->CR & RCC_CR_HSI48ON));
    }
#endif
    if (all || RCC->CR & RCC_CR_HSEON) {
        streamer_printf(streamer, "  HSE: %s %s%s\n", on_off_state(RCC->CR & RCC_CR_HSEON),
                        freq_str(HSE_VALUE, buf), (RCC->CR & RCC_CR_HSEBYP) ? " bypass" : "");
    }
    if (all || RCC->CR & RCC_CR_PLLON) {
        uint32_t pll_in = pll_src == 2 ? HSI_VALUE : pll_src == 3 ? HSE_VALUE : 0;
        uint32_t m = ((pllcfgr & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos) + 1;
        uint32_t n = (pllcfgr & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos;
        uint32_t vco = pll_in / m * n;

        streamer_printf(streamer, "  PLL: %s src: %s M %u N %u VCO %s\n", on_off_state(RCC->CR & RCC_CR_PLLON),
                        pll_source[pll_src], (unsigned int)m, (unsigned int)n, freq_str(vco, buf));
        streamer_printf(streamer, "    PLLP: %s %s\n", on_off_state(pllcfgr & RCC_PLLCFGR_PLLPEN),
                        freq_str(vco / (((pllcfgr & RCC_PLLCFGR_PLLP) >> RCC_PLLCFGR_PLLP_Pos) + 1), buf));
#ifdef RCC_PLLCFGR_PLLQEN
        streamer_printf(streamer, "    PLLQ: %s %s\n", on_off_state(pllcfgr & RCC_PLLCFGR_PLLQEN),
                        freq_str(vco / (((pllcfgr & RCC_PLLCFGR_PLLQ) >> RCC_PLLCFGR_PLLQ_Pos) + 1), buf));
#endif
        streamer_printf(streamer, "    PLLR: %s %s\n", on_off_state(pllcfgr & RCC_PLLCFGR_PLLREN),
                        freq_str(vco / (((pllcfgr & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos) + 1), buf));
    }
    if (all || RCC->CSR & RCC_CSR_LSION) {
        streamer_printf(streamer, "  LSI: %s\n", on_off_state(RCC->CSR & RCC_CSR_LSION));
    }
    if (all || RCC->BDCR & RCC_BDCR_LSEON) {
        streamer_printf(streamer, "  LSE: %s\n", on_off_state(RCC->BDCR & RCC_BDCR_LSEON));
    }
    if (all || RCC->BDCR & RCC_BDCR_RTCEN) {
        streamer_printf(streamer, "  RTC: %s %s %s\n", on_off_state(RCC->BDCR & RCC_BDCR_RTCEN),
                        rtc_source[(RCC->BDCR & RCC_BDCR_RTCSEL) >> RCC_BDCR_RTCSEL_Pos],
                        freq_str(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_RTC), buf));
    }
    streamer_printf(streamer, "  SYSCLK: %s\n", freq_str(SystemCoreClock, buf));
    streamer_printf(streamer, "    source: %s\n", system_clock_source[sw]);
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

static const struct shell_param mcu_cli_timingr_params[] = {
    {"<value>", "new TIMINGR register value"},
    {"[1|2|3]", "I2C instance, default 1"},
    {NULL, NULL}
};

static const struct shell_cmd_help mcu_cli_timingr_help = {
    .summary = "set I2C TIMINGR register",
    .usage = "\n"
             "timingr 0x00303D5B 2\n"
             "  Sets TIMINGR of I2C2.\n",
    .params = mcu_cli_timingr_params,
};
#endif

static int
mcu_cli_info_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                 struct streamer *streamer)
{
    char buf[20];
    bool all;

    all = argc > 1 && strcmp(argv[1], "all") == 0;

    print_clocks(streamer, all);
    streamer_printf(streamer, "Peripherals:\n");
    print_ahb_peripherals(streamer, all);
    streamer_printf(streamer, "  APB PCLK: %s\n", freq_str(HAL_RCC_GetPCLK1Freq(), buf));
    print_apb1_peripherals(streamer, all);
    print_apb2_peripherals(streamer, all);

    return 0;
}

static int
mcu_cli_timingr_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                    struct streamer *streamer)
{
    static I2C_TypeDef *const i2cs[] = {
        I2C1,
        I2C2,
#ifdef I2C3
        I2C3,
#endif
    };
    I2C_TypeDef *i2c;
    uint32_t timingr;
    int num = 1;
    int rc;

    if (argc < 2) {
        streamer_printf(streamer, "timingr <value> [i2c]\n");
        return 0;
    }
    timingr = (uint32_t)parse_ull(argv[1], &rc);
    if (rc) {
        streamer_printf(streamer, "Invalid value %s\n", argv[1]);
        return 0;
    }
    if (argc > 2) {
        num = (int)parse_ll_bounds(argv[2], 1, ARRAY_SIZE(i2cs), &rc);
        if (rc) {
            streamer_printf(streamer, "Invalid I2C instance %s\n", argv[2]);
            return 0;
        }
    }
    i2c = i2cs[num - 1];
    i2c->CR1 &= ~I2C_CR1_PE;
    i2c->TIMINGR = timingr;
    i2c->CR1 |= I2C_CR1_PE;

    return 0;
}

static const struct shell_cmd mcu_cli_commands[] = {
    SHELL_CMD_EXT("info", mcu_cli_info_cmd, &mcu_cli_info_help),
    SHELL_CMD_EXT("timingr", mcu_cli_timingr_cmd, &mcu_cli_timingr_help),
};

SHELL_MODULE_WITH_TABLE(mcu, mcu_cli_commands)
