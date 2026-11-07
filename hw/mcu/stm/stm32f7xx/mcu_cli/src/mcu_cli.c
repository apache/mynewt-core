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
#include <stm32f7xx_hal.h>
#include <parse/parse.h>
#include <shell/shell.h>

extern uint32_t SystemCoreClock;

static const char *system_clock_source[4] = { "HSI", "HSE", "PLL", "?" };
static const char *pll_source[2] = { "HSI", "HSE" };
static const char *rtc_source[4] = { "---", "LSE", "LSI", "HSE" };
/* RCC_DCKCFGR2 USARTxSEL / I2CxSEL encoding */
static const char *uart_clk_src[4] = { "PCLK", "SYSCLK", "HSI", "LSE" };

static const char *
on_off_state(uint32_t on)
{
    return on ? "on" : "off";
}

static char *
freq_str(uint32_t freq, char buf[])
{
    const bool mega = freq >= 1000000;
    unsigned int div;
    char *p;

    if (freq == 0) {
        strcpy(buf, "---");
    } else if (freq >= 1000) {
        div = mega ? 1000000 : 1000;
        p = buf - 1 +
            sprintf(buf, mega ? "%u.%06u" : "%u.%03u",
                    (unsigned int)freq / div, (unsigned int)freq % div);
        /* Skip trailing 0 */
        while (*p == '0') {
            --p;
        }
        /* If ends with '.' overwrite '.' */
        strcpy(*p == '.' ? p : p + 1, mega ? " MHz" : " kHz");
    } else {
        sprintf(buf, "%d Hz", (int)freq);
    }
    return buf;
}

/*
 * Timer kernel clock for APB bus.
 * ppre - APB prescaler field (RCC_CFGR PPRE1 or PPRE2) value 0..7.
 */
static uint32_t
tim_clock(uint32_t pclk, uint32_t ppre)
{
    /* 0xx: APB clock not divided, timers run at PCLK */
    if (ppre < 4) {
        return pclk;
    }
#ifdef RCC_DCKCFGR1_TIMPRE
    if (RCC->DCKCFGR1 & RCC_DCKCFGR1_TIMPRE) {
        /* APB prescaler 2 or 4: HCLK, higher: 4 x PCLK */
        return ppre <= 5 ? HAL_RCC_GetHCLKFreq() : pclk * 4;
    }
#endif
    return pclk * 2;
}

/*
 * USART and I2C kernel clock selected by 2 bit field of RCC_DCKCFGR2
 * (PCLK, SYSCLK, HSI, LSE).
 */
static uint32_t
kernel_clock(uint32_t sel, uint32_t pclk)
{
    switch (sel) {
    case 0:
        return pclk;
    case 1:
        return SystemCoreClock;
    case 2:
        return HSI_VALUE;
    default:
        return LSE_VALUE;
    }
}

#define DCKCFGR2_SEL(pos) ((RCC->DCKCFGR2 >> (pos)) & 3)

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
stream_tim(struct streamer *streamer, TIM_TypeDef *tim, const char *name,
           uint32_t base_freq, bool on)
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
stream_spi(struct streamer *streamer, SPI_TypeDef *spi, const char *name,
           uint32_t pclk, bool on)
{
    char buf[20];

    if (stream_off(streamer, name, on)) {
        return;
    }
    streamer_printf(
        streamer, "    %-7s on %s%s\n", name,
        freq_str(pclk >> (1 + ((spi->CR1 & SPI_CR1_BR) >> SPI_CR1_BR_Pos)), buf),
        (spi->CR1 & SPI_CR1_SPE) ? "" : " disabled");
}

static uint32_t
i2c_freq(uint32_t i2c_clock, uint32_t timingr)
{
    if (timingr == 0) {
        return 0;
    }
    return i2c_clock /
           ((1 + ((timingr & I2C_TIMINGR_PRESC) >> I2C_TIMINGR_PRESC_Pos))) /
           ((((timingr & I2C_TIMINGR_SCLL) >> I2C_TIMINGR_SCLL_Pos) + 1 + 4 +
             ((timingr & I2C_TIMINGR_SCLH) >> I2C_TIMINGR_SCLH_Pos) + 1));
}

static void
stream_i2c(struct streamer *streamer, I2C_TypeDef *i2c, const char *name,
           uint32_t sel_pos, bool on)
{
    char buf[20];
    uint32_t clk;

    if (stream_off(streamer, name, on)) {
        return;
    }
    clk = kernel_clock(DCKCFGR2_SEL(sel_pos), HAL_RCC_GetPCLK1Freq());
    streamer_printf(streamer, "    %-7s on %s (TIMINGR 0x%08X)%s\n", name,
                    freq_str(i2c_freq(clk, i2c->TIMINGR), buf),
                    (unsigned int)i2c->TIMINGR,
                    (i2c->CR1 & I2C_CR1_PE) ? "" : " disabled");
}

static void
stream_usart(struct streamer *streamer, USART_TypeDef *usart, const char *name,
             uint32_t sel_pos, uint32_t pclk, bool on)
{
    uint32_t sel;
    uint32_t clk;
    uint32_t brr;
    uint32_t baud = 0;

    if (stream_off(streamer, name, on)) {
        return;
    }
    sel = DCKCFGR2_SEL(sel_pos);
    clk = kernel_clock(sel, pclk);
    brr = usart->BRR & 0xFFFF;
    if (usart->CR1 & USART_CR1_OVER8) {
        brr = (brr & ~0xFU) | ((brr & 7) << 1);
        clk *= 2;
    }
    if (brr) {
        baud = clk / brr;
    }
    if (usart->CR1 & USART_CR1_UE) {
        streamer_printf(streamer, "    %-7s on %u bps (%s)\n", name,
                        (unsigned int)baud, uart_clk_src[sel]);
    } else {
        streamer_printf(streamer, "    %-7s on (%s) disabled\n", name,
                        uart_clk_src[sel]);
    }
}

static void
print_ahb_peripherals(struct streamer *streamer, bool all)
{
    char buf[20];

    streamer_printf(streamer, "  AHB HCLK: %s\n",
                    freq_str(HAL_RCC_GetHCLKFreq(), buf));

    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN) {
        streamer_printf(streamer, "    GPIOA   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN) {
        streamer_printf(streamer, "    GPIOB   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOCEN) {
        streamer_printf(streamer, "    GPIOC   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOCEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIODEN) {
        streamer_printf(streamer, "    GPIOD   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIODEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOEEN) {
        streamer_printf(streamer, "    GPIOE   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOEEN));
    }
#ifdef RCC_AHB1ENR_GPIOFEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOFEN) {
        streamer_printf(streamer, "    GPIOF   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOFEN));
    }
#endif
#ifdef RCC_AHB1ENR_GPIOGEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOGEN) {
        streamer_printf(streamer, "    GPIOG   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOGEN));
    }
#endif
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOHEN) {
        streamer_printf(streamer, "    GPIOH   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOHEN));
    }
#ifdef RCC_AHB1ENR_GPIOIEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOIEN) {
        streamer_printf(streamer, "    GPIOI   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOIEN));
    }
#endif
#ifdef RCC_AHB1ENR_GPIOJEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOJEN) {
        streamer_printf(streamer, "    GPIOJ   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOJEN));
    }
#endif
#ifdef RCC_AHB1ENR_GPIOKEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOKEN) {
        streamer_printf(streamer, "    GPIOK   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOKEN));
    }
#endif
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_CRCEN) {
        streamer_printf(streamer, "    CRC     %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_CRCEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_BKPSRAMEN) {
        streamer_printf(streamer, "    BKPSRAM %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_BKPSRAMEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_DTCMRAMEN) {
        streamer_printf(streamer, "    DTCMRAM %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_DTCMRAMEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN) {
        streamer_printf(streamer, "    DMA1    %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_DMA2EN) {
        streamer_printf(streamer, "    DMA2    %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_DMA2EN));
    }
#ifdef RCC_AHB1ENR_DMA2DEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_DMA2DEN) {
        streamer_printf(streamer, "    DMA2D   %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_DMA2DEN));
    }
#endif
#ifdef RCC_AHB1ENR_ETHMACEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_ETHMACEN) {
        streamer_printf(streamer, "    ETHMAC  %s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_ETHMACEN));
    }
#endif
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_OTGHSEN) {
        streamer_printf(streamer, "    OTGHS   %s%s\n",
                        on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_OTGHSEN),
                        (RCC->AHB1ENR & RCC_AHB1ENR_OTGHSULPIEN) ? " ULPI" : "");
    }
#ifdef RCC_AHB2ENR_DCMIEN
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_DCMIEN) {
        streamer_printf(streamer, "    DCMI    %s\n",
                        on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_DCMIEN));
    }
#endif
#ifdef RCC_AHB2ENR_JPEGEN
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_JPEGEN) {
        streamer_printf(streamer, "    JPEG    %s\n",
                        on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_JPEGEN));
    }
#endif
#ifdef RCC_AHB2ENR_AESEN
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_AESEN) {
        streamer_printf(streamer, "    AES     %s\n",
                        on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_AESEN));
    }
#endif
#ifdef RCC_AHB2ENR_CRYPEN
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN) {
        streamer_printf(streamer, "    CRYP    %s\n",
                        on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN));
    }
#endif
#ifdef RCC_AHB2ENR_HASHEN
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_HASHEN) {
        streamer_printf(streamer, "    HASH    %s\n",
                        on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_HASHEN));
    }
#endif
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_RNGEN) {
        streamer_printf(streamer, "    RNG     %s\n",
                        on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_RNGEN));
    }
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) {
        streamer_printf(streamer, "    OTGFS   %s\n",
                        on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN));
    }
    if (all || RCC->AHB3ENR & RCC_AHB3ENR_FMCEN) {
        streamer_printf(streamer, "    FMC     %s\n",
                        on_off_state(RCC->AHB3ENR & RCC_AHB3ENR_FMCEN));
    }
    if ((all || RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN) &&
        !stream_off(streamer, "QSPI", RCC->AHB3ENR & RCC_AHB3ENR_QSPIEN)) {
        uint32_t presc =
            (QUADSPI->CR & QUADSPI_CR_PRESCALER) >> QUADSPI_CR_PRESCALER_Pos;

        streamer_printf(streamer, "    QSPI    on %s%s\n",
                        freq_str(HAL_RCC_GetHCLKFreq() / (presc + 1), buf),
                        (QUADSPI->CR & QUADSPI_CR_EN) ? "" : " disabled");
    }
}

static void
print_apb1_peripherals(struct streamer *streamer, bool all)
{
    char buf[20];
    uint32_t pclk = HAL_RCC_GetPCLK1Freq();
    uint32_t timclk =
        tim_clock(pclk, (RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos);

    streamer_printf(streamer, "  APB1 PCLK1: %s\n", freq_str(pclk, buf));
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM2EN) {
        stream_tim(streamer, TIM2, "TIM2", timclk, RCC->APB1ENR & RCC_APB1ENR_TIM2EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM3EN) {
        stream_tim(streamer, TIM3, "TIM3", timclk, RCC->APB1ENR & RCC_APB1ENR_TIM3EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM4EN) {
        stream_tim(streamer, TIM4, "TIM4", timclk, RCC->APB1ENR & RCC_APB1ENR_TIM4EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM5EN) {
        stream_tim(streamer, TIM5, "TIM5", timclk, RCC->APB1ENR & RCC_APB1ENR_TIM5EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM6EN) {
        stream_tim(streamer, TIM6, "TIM6", timclk, RCC->APB1ENR & RCC_APB1ENR_TIM6EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM7EN) {
        stream_tim(streamer, TIM7, "TIM7", timclk, RCC->APB1ENR & RCC_APB1ENR_TIM7EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM12EN) {
        stream_tim(streamer, TIM12, "TIM12", timclk, RCC->APB1ENR & RCC_APB1ENR_TIM12EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM13EN) {
        stream_tim(streamer, TIM13, "TIM13", timclk, RCC->APB1ENR & RCC_APB1ENR_TIM13EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM14EN) {
        stream_tim(streamer, TIM14, "TIM14", timclk, RCC->APB1ENR & RCC_APB1ENR_TIM14EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_LPTIM1EN) {
        streamer_printf(streamer, "    LPTIM1  %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_LPTIM1EN));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_WWDGEN) {
        streamer_printf(streamer, "    WWDG    %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_WWDGEN));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_SPI2EN) {
        stream_spi(streamer, SPI2, "SPI2", pclk, RCC->APB1ENR & RCC_APB1ENR_SPI2EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_SPI3EN) {
        stream_spi(streamer, SPI3, "SPI3", pclk, RCC->APB1ENR & RCC_APB1ENR_SPI3EN);
    }
#ifdef RCC_APB1ENR_SPDIFRXEN
    if (all || RCC->APB1ENR & RCC_APB1ENR_SPDIFRXEN) {
        streamer_printf(streamer, "    SPDIFRX %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_SPDIFRXEN));
    }
#endif
    if (all || RCC->APB1ENR & RCC_APB1ENR_USART2EN) {
        stream_usart(streamer, USART2, "USART2", RCC_DCKCFGR2_USART2SEL_Pos,
                     pclk, RCC->APB1ENR & RCC_APB1ENR_USART2EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_USART3EN) {
        stream_usart(streamer, USART3, "USART3", RCC_DCKCFGR2_USART3SEL_Pos,
                     pclk, RCC->APB1ENR & RCC_APB1ENR_USART3EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_UART4EN) {
        stream_usart(streamer, UART4, "UART4", RCC_DCKCFGR2_UART4SEL_Pos, pclk,
                     RCC->APB1ENR & RCC_APB1ENR_UART4EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_UART5EN) {
        stream_usart(streamer, UART5, "UART5", RCC_DCKCFGR2_UART5SEL_Pos, pclk,
                     RCC->APB1ENR & RCC_APB1ENR_UART5EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_UART7EN) {
        stream_usart(streamer, UART7, "UART7", RCC_DCKCFGR2_UART7SEL_Pos, pclk,
                     RCC->APB1ENR & RCC_APB1ENR_UART7EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_UART8EN) {
        stream_usart(streamer, UART8, "UART8", RCC_DCKCFGR2_UART8SEL_Pos, pclk,
                     RCC->APB1ENR & RCC_APB1ENR_UART8EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_I2C1EN) {
        stream_i2c(streamer, I2C1, "I2C1", RCC_DCKCFGR2_I2C1SEL_Pos,
                   RCC->APB1ENR & RCC_APB1ENR_I2C1EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_I2C2EN) {
        stream_i2c(streamer, I2C2, "I2C2", RCC_DCKCFGR2_I2C2SEL_Pos,
                   RCC->APB1ENR & RCC_APB1ENR_I2C2EN);
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_I2C3EN) {
        stream_i2c(streamer, I2C3, "I2C3", RCC_DCKCFGR2_I2C3SEL_Pos,
                   RCC->APB1ENR & RCC_APB1ENR_I2C3EN);
    }
#ifdef RCC_APB1ENR_I2C4EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_I2C4EN) {
        stream_i2c(streamer, I2C4, "I2C4", RCC_DCKCFGR2_I2C4SEL_Pos,
                   RCC->APB1ENR & RCC_APB1ENR_I2C4EN);
    }
#endif
    if (all || RCC->APB1ENR & RCC_APB1ENR_CAN1EN) {
        streamer_printf(streamer, "    CAN1    %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_CAN1EN));
    }
#ifdef RCC_APB1ENR_CAN2EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_CAN2EN) {
        streamer_printf(streamer, "    CAN2    %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_CAN2EN));
    }
#endif
#ifdef RCC_APB1ENR_CAN3EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_CAN3EN) {
        streamer_printf(streamer, "    CAN3    %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_CAN3EN));
    }
#endif
#ifdef RCC_APB1ENR_CECEN
    if (all || RCC->APB1ENR & RCC_APB1ENR_CECEN) {
        streamer_printf(streamer, "    CEC     %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_CECEN));
    }
#endif
    if ((all || RCC->APB1ENR & RCC_APB1ENR_PWREN) &&
        !stream_off(streamer, "PWR", RCC->APB1ENR & RCC_APB1ENR_PWREN)) {
        streamer_printf(streamer, "    PWR     on VOS scale %u%s\n",
                        4 - (unsigned int)((PWR->CR1 & PWR_CR1_VOS) >> PWR_CR1_VOS_Pos),
                        (PWR->CR1 & PWR_CR1_ODEN) ? " overdrive" : "");
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_DACEN) {
        streamer_printf(streamer, "    DAC     %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_DACEN));
    }
}

static void
print_apb2_peripherals(struct streamer *streamer, bool all)
{
    char buf[20];
    uint32_t pclk = HAL_RCC_GetPCLK2Freq();
    uint32_t timclk =
        tim_clock(pclk, (RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos);
    uint32_t adcclk =
        pclk / ((((ADC->CCR & ADC_CCR_ADCPRE) >> ADC_CCR_ADCPRE_Pos) + 1) * 2);

    streamer_printf(streamer, "  APB2 PCLK2: %s\n", freq_str(pclk, buf));
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM1EN) {
        stream_tim(streamer, TIM1, "TIM1", timclk, RCC->APB2ENR & RCC_APB2ENR_TIM1EN);
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM8EN) {
        stream_tim(streamer, TIM8, "TIM8", timclk, RCC->APB2ENR & RCC_APB2ENR_TIM8EN);
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_USART1EN) {
        stream_usart(streamer, USART1, "USART1", RCC_DCKCFGR2_USART1SEL_Pos,
                     pclk, RCC->APB2ENR & RCC_APB2ENR_USART1EN);
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_USART6EN) {
        stream_usart(streamer, USART6, "USART6", RCC_DCKCFGR2_USART6SEL_Pos,
                     pclk, RCC->APB2ENR & RCC_APB2ENR_USART6EN);
    }
    if ((all || RCC->APB2ENR & RCC_APB2ENR_ADC1EN) &&
        !stream_off(streamer, "ADC1", RCC->APB2ENR & RCC_APB2ENR_ADC1EN)) {
        streamer_printf(streamer, "    ADC1    on %s\n", freq_str(adcclk, buf));
    }
    if ((all || RCC->APB2ENR & RCC_APB2ENR_ADC2EN) &&
        !stream_off(streamer, "ADC2", RCC->APB2ENR & RCC_APB2ENR_ADC2EN)) {
        streamer_printf(streamer, "    ADC2    on %s\n", freq_str(adcclk, buf));
    }
    if ((all || RCC->APB2ENR & RCC_APB2ENR_ADC3EN) &&
        !stream_off(streamer, "ADC3", RCC->APB2ENR & RCC_APB2ENR_ADC3EN)) {
        streamer_printf(streamer, "    ADC3    on %s\n", freq_str(adcclk, buf));
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_SDMMC1EN) {
        streamer_printf(streamer, "    SDMMC1  %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_SDMMC1EN));
    }
#ifdef RCC_APB2ENR_SDMMC2EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_SDMMC2EN) {
        streamer_printf(streamer, "    SDMMC2  %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_SDMMC2EN));
    }
#endif
    if (all || RCC->APB2ENR & RCC_APB2ENR_SPI1EN) {
        stream_spi(streamer, SPI1, "SPI1", pclk, RCC->APB2ENR & RCC_APB2ENR_SPI1EN);
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_SPI4EN) {
        stream_spi(streamer, SPI4, "SPI4", pclk, RCC->APB2ENR & RCC_APB2ENR_SPI4EN);
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) {
        streamer_printf(streamer, "    SYSCFG  %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN));
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM9EN) {
        stream_tim(streamer, TIM9, "TIM9", timclk, RCC->APB2ENR & RCC_APB2ENR_TIM9EN);
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM10EN) {
        stream_tim(streamer, TIM10, "TIM10", timclk, RCC->APB2ENR & RCC_APB2ENR_TIM10EN);
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM11EN) {
        stream_tim(streamer, TIM11, "TIM11", timclk, RCC->APB2ENR & RCC_APB2ENR_TIM11EN);
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_SPI5EN) {
        stream_spi(streamer, SPI5, "SPI5", pclk, RCC->APB2ENR & RCC_APB2ENR_SPI5EN);
    }
#ifdef RCC_APB2ENR_SPI6EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_SPI6EN) {
        stream_spi(streamer, SPI6, "SPI6", pclk, RCC->APB2ENR & RCC_APB2ENR_SPI6EN);
    }
#endif
    if ((all || RCC->APB2ENR & RCC_APB2ENR_SAI1EN) &&
        !stream_off(streamer, "SAI1", RCC->APB2ENR & RCC_APB2ENR_SAI1EN)) {
        streamer_printf(streamer, "    SAI1    on %s\n",
                        freq_str(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI1), buf));
    }
    if ((all || RCC->APB2ENR & RCC_APB2ENR_SAI2EN) &&
        !stream_off(streamer, "SAI2", RCC->APB2ENR & RCC_APB2ENR_SAI2EN)) {
        streamer_printf(streamer, "    SAI2    on %s\n",
                        freq_str(HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_SAI2), buf));
    }
#ifdef RCC_APB2ENR_LTDCEN
    if (all || RCC->APB2ENR & RCC_APB2ENR_LTDCEN) {
        streamer_printf(streamer, "    LTDC    %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_LTDCEN));
    }
#endif
#ifdef RCC_APB2ENR_DSIEN
    if (all || RCC->APB2ENR & RCC_APB2ENR_DSIEN) {
        streamer_printf(streamer, "    DSI     %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_DSIEN));
    }
#endif
#ifdef RCC_APB2ENR_DFSDM1EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_DFSDM1EN) {
        streamer_printf(streamer, "    DFSDM1  %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_DFSDM1EN));
    }
#endif
#ifdef RCC_APB2ENR_MDIOEN
    if (all || RCC->APB2ENR & RCC_APB2ENR_MDIOEN) {
        streamer_printf(streamer, "    MDIO    %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_MDIOEN));
    }
#endif
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
        streamer_printf(streamer, "  HSI: %s %s\n",
                        on_off_state(RCC->CR & RCC_CR_HSION),
                        freq_str(HSI_VALUE, buf));
    }
    if (all || RCC->CR & RCC_CR_HSEON) {
        streamer_printf(streamer, "  HSE: %s %s%s\n",
                        on_off_state(RCC->CR & RCC_CR_HSEON),
                        freq_str(HSE_VALUE, buf),
                        (RCC->CR & RCC_CR_HSEBYP) ? " bypass" : "");
    }
    if (all || RCC->CR & RCC_CR_PLLON) {
        uint32_t pll_in = pll_src ? HSE_VALUE : HSI_VALUE;
        uint32_t m = (pllcfgr & RCC_PLLCFGR_PLLM) >> RCC_PLLCFGR_PLLM_Pos;
        uint32_t n = (pllcfgr & RCC_PLLCFGR_PLLN) >> RCC_PLLCFGR_PLLN_Pos;
        uint32_t p =
            (((pllcfgr & RCC_PLLCFGR_PLLP) >> RCC_PLLCFGR_PLLP_Pos) + 1) * 2;
        uint32_t q = (pllcfgr & RCC_PLLCFGR_PLLQ) >> RCC_PLLCFGR_PLLQ_Pos;
        uint32_t vco = m ? pll_in / m * n : 0;

        streamer_printf(streamer, "  PLL: %s src: %s M %u N %u VCO %s\n",
                        on_off_state(RCC->CR & RCC_CR_PLLON), pll_source[pll_src],
                        (unsigned int)m, (unsigned int)n, freq_str(vco, buf));
        streamer_printf(streamer, "    PLLP: %s\n", freq_str(vco / p, buf));
        streamer_printf(streamer, "    PLLQ: %s\n", freq_str(q ? vco / q : 0, buf));
#ifdef RCC_PLLCFGR_PLLR
        {
            uint32_t r = (pllcfgr & RCC_PLLCFGR_PLLR) >> RCC_PLLCFGR_PLLR_Pos;

            streamer_printf(streamer, "    PLLR: %s\n",
                            freq_str(r ? vco / r : 0, buf));
        }
#endif
    }
    if (all || RCC->CR & RCC_CR_PLLI2SON) {
        streamer_printf(streamer, "  PLLI2S: %s\n",
                        on_off_state(RCC->CR & RCC_CR_PLLI2SON));
    }
    if (all || RCC->CR & RCC_CR_PLLSAION) {
        streamer_printf(streamer, "  PLLSAI: %s\n",
                        on_off_state(RCC->CR & RCC_CR_PLLSAION));
    }
    if (all || RCC->CSR & RCC_CSR_LSION) {
        streamer_printf(streamer, "  LSI: %s\n",
                        on_off_state(RCC->CSR & RCC_CSR_LSION));
    }
    if (all || RCC->BDCR & RCC_BDCR_LSEON) {
        streamer_printf(streamer, "  LSE: %s\n",
                        on_off_state(RCC->BDCR & RCC_BDCR_LSEON));
    }
    if (all || RCC->BDCR & RCC_BDCR_RTCEN) {
        streamer_printf(
            streamer, "  RTC: %s %s\n", on_off_state(RCC->BDCR & RCC_BDCR_RTCEN),
            rtc_source[(RCC->BDCR & RCC_BDCR_RTCSEL) >> RCC_BDCR_RTCSEL_Pos]);
    }
    streamer_printf(streamer, "  SYSCLK: %s\n", freq_str(SystemCoreClock, buf));
    streamer_printf(streamer, "    source: %s\n", system_clock_source[sw]);
    streamer_printf(streamer, "  FLASH latency %u\n",
                    (unsigned int)((FLASH->ACR & FLASH_ACR_LATENCY) >>
                                   FLASH_ACR_LATENCY_Pos));
}

#if MYNEWT_VAL(SHELL_CMD_HELP)
static const struct shell_param mcu_cli_info_params[] = {
    { "all", "show also disabled peripherals." },
    { NULL }
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
    { "<value>", "new TIMINGR register value" },
    { "[1|2|3|4]", "I2C instance, default 1" },
    { NULL }
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
    bool all;

    all = argc > 1 && strcmp(argv[1], "all") == 0;

    print_clocks(streamer, all);
    streamer_printf(streamer, "Peripherals:\n");
    print_ahb_peripherals(streamer, all);
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
        I2C3,
#ifdef I2C4
        I2C4,
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
