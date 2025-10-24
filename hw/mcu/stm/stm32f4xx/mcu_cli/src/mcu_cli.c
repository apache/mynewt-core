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
#include <stm32f4xx_hal.h>
#include <mcu/clock_stm32f4xx.h>
#include <shell/shell.h>

extern uint32_t SystemCoreClock;

static const char *system_clock_source[4] = { "HSI", "HSE", "PLL", "" };

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

static void
print_ahb_peripherals(struct streamer *streamer, bool all)
{
    char freq_buf[20];

    streamer_printf(streamer, "  AHB HCLK: %s\n",
                    freq_str(HAL_RCC_GetHCLKFreq(), freq_buf));

    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN) {
        streamer_printf(streamer, "    GPIOA  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOAEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN) {
        streamer_printf(streamer, "    GPIOB  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOBEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOCEN) {
        streamer_printf(streamer, "    GPIOC  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOCEN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIODEN) {
        streamer_printf(streamer, "    GPIOD  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIODEN));
    }
#ifdef GPIOE
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOEEN) {
        streamer_printf(streamer, "    GPIOE  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOEEN));
    }
#endif
#ifdef GPIOF
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOFEN) {
        streamer_printf(streamer, "    GPIOF  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOFEN));
    }
#endif
#ifdef GPIOG
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOGEN) {
        streamer_printf(streamer, "    GPIOG  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOGEN));
    }
#endif
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOHEN) {
        streamer_printf(streamer, "    GPIOH  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOHEN));
    }
#ifdef RCC_AHB1ENR_GPIOIEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOIEN) {
        streamer_printf(streamer, "    GPIOI  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOIEN));
    }
#endif
#ifdef RCC_AHB1ENR_GPIOJEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOJEN) {
        streamer_printf(streamer, "    GPIOI %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOJEN));
    }
#endif
#ifdef RCC_AHB1ENR_GPIOKEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_GPIOKEN) {
        streamer_printf(streamer, "    GPIOI %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_GPIOKEN));
    }
#endif
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_CRCEN) {
        streamer_printf(streamer, "    CRC    %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_CRCEN));
    }
#ifdef RCC_AHB1ENR_BKPSRAMEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_BKPSRAMEN) {
        streamer_printf(streamer, "    BKPSRAM %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_BKPSRAMEN));
    }
#endif
#ifdef RCC_AHB1ENR_CCMDATARAMEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_CCMDATARAMEN) {
        streamer_printf(streamer, "    CCMDATARAM %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_CCMDATARAMEN));
    }
#endif
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN) {
        streamer_printf(streamer, "    DMA1   %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_DMA1EN));
    }
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_DMA2EN) {
        streamer_printf(streamer, "    DMA2   %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_DMA2EN));
    }
#ifdef RCC_AHB1ENR_ETHMACEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_ETHMACEN) {
        streamer_printf(streamer, "    ETHMAC %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_ETHMACEN));
    }
#endif
#ifdef RCC_AHB1ENR_ETHMACTXEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_ETHMACTXEN) {
        streamer_printf(streamer, "    ETHMACTX %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_ETHMACTXEN));
    }
#endif
#ifdef RCC_AHB1ENR_ETHMACRXEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_ETHMACRXEN) {
        streamer_printf(streamer, "    ETHMACRX %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_ETHMACRXEN));
    }
#endif
#ifdef RCC_AHB1ENR_ETHMACPTPEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_ETHMACPTPEN) {
        streamer_printf(streamer, "    ETHMACPTP %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_ETHMACPTPEN));
    }
#endif
#ifdef RCC_AHB1ENR_OTGHSEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_OTGHSEN) {
        streamer_printf(streamer, "    OTGHS  %s\n", on_off_state(RCC->AHB1ENR & RCC_AHB1ENR_OTGHSEN));
    }
#endif
#ifdef RCC_AHB1ENR_OTGHSULPIEN
    if (all || RCC->AHB1ENR & RCC_AHB1ENR_OTGHSULPIEN) {
        streamer_printf(streamer, "    OTGHSULPI %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB1ENR_OTGHSULPIEN));
    }
#endif

#if defined(RCC_AHB2ENR_DCMIEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_DCMIEN) {
        streamer_printf(streamer, "    DCMI   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_DCMIEN));
    }
#endif
#if defined(RCC_AHB2ENR_AESEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_AESEN) {
        streamer_printf(streamer, "    AES    %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_AESEN));
    }
#endif
#if defined(RCC_AHB2ENR_CRYPEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN) {
        streamer_printf(streamer, "    CRYP   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_CRYPEN));
    }
#endif
#if defined(RCC_AHB2ENR_HASHEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_HASHEN) {
        streamer_printf(streamer, "    HASH   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_HASHEN));
    }
#endif
#if defined(RCC_AHB2ENR_RNGEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_RNGEN) {
        streamer_printf(streamer, "    RNG    %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_RNGEN));
    }
#endif
#if defined(RCC_AHB2ENR_OTGFSEN)
    if (all || RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN) {
        streamer_printf(streamer, "    OTGFS  %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB2ENR_OTGFSEN));
    }
#endif
#if defined(RCC_AHB3ENR_FSMCEN)
    if (all || RCC->AHB3ENR & RCC_AHB3ENR_FSMCEN) {
        streamer_printf(streamer, "    FSMC   %s\n", on_off_state(RCC->AHB2ENR & RCC_AHB3ENR_FSMCEN));
    }
#endif
}

static void
print_apb1_peripherals(struct streamer *streamer, bool all)
{
    uint32_t pckl1 = HAL_RCC_GetPCLK1Freq();
    uint32_t timmul = RCC->CFGR & RCC_CFGR_PPRE2_2 ? 2 : 1;
    char freq_buf[20];

    streamer_printf(streamer, "  APB1 PCLK1: %s\n", freq_str(pckl1, freq_buf));
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM2EN) {
        streamer_printf(streamer, "    TIM2   %s %s (ARR %u)\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM2EN),
                        freq_str(pckl1 * timmul / (TIM2->PSC + 1), freq_buf),
                        (TIM2->ARR));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM3EN) {
        streamer_printf(streamer, "    TIM3   %s %s (ARR %u)\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM3EN),
                        freq_str(pckl1 * timmul / (TIM3->PSC + 1), freq_buf),
                        (TIM3->ARR + 1));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM4EN) {
        streamer_printf(streamer, "    TIM4   %s %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM4EN),
                        freq_str(pckl1 * timmul / (TIM4->PSC + 1), freq_buf));
    }
#ifdef RCC_APB1ENR_TIM5EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM5EN) {
        streamer_printf(streamer, "    TIM5   %s %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM5EN),
                        freq_str(pckl1 * timmul / (TIM5->PSC + 1), freq_buf));
    }
#endif
#ifdef RCC_APB1ENR_TIM6EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM6EN) {
        streamer_printf(streamer, "    TIM6   %s %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM6EN),
                        freq_str(pckl1 * timmul / (TIM6->PSC + 1), freq_buf));
    }
#endif
#ifdef RCC_APB1ENR_TIM7EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM7EN) {
        streamer_printf(streamer, "    TIM7   %s %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM7EN),
                        freq_str(pckl1 * timmul / (TIM7->PSC + 1), freq_buf));
    }
#endif
#ifdef RCC_APB1ENR_TIM12EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM12EN) {
        streamer_printf(streamer, "    TIM12  %s %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM12EN),
                        freq_str(pckl1 * timmul / (TIM12->PSC + 1), freq_buf));
    }
#endif
#ifdef RCC_APB1ENR_TIM13EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM13EN) {
        streamer_printf(streamer, "    TIM13  %s %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM13EN),
                        freq_str(pckl1 * timmul / (TIM3->PSC + 1), freq_buf));
    }
#endif
#ifdef RCC_APB1ENR_TIM14EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM14EN) {
        streamer_printf(streamer, "    TIM14  %s %s\n",
                        on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM14EN),
                        freq_str(pckl1 * timmul / (TIM14->PSC + 1), freq_buf));
    }
#endif
    if (all || RCC->APB1ENR & RCC_APB1ENR_WWDGEN) {
        streamer_printf(streamer, "    WWD    %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_WWDGEN));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_SPI2EN) {
        streamer_printf(
            streamer, "    SPI2   %s %s\n",
            on_off_state(RCC->APB1ENR & RCC_APB1ENR_SPI2EN),
            freq_str(pckl1 >> (1 + ((SPI2->CR1 & SPI_CR1_BR_Msk) >> SPI_CR1_BR_Pos)),
                     freq_buf));
    }
#ifdef RCC_APB1ENR_SPI3EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_SPI3EN) {
        streamer_printf(
            streamer, "    SPI3   %s %s\n",
            on_off_state(RCC->APB1ENR & RCC_APB1ENR_SPI3EN),
            freq_str(pckl1 >> (1 + ((SPI3->CR1 & SPI_CR1_BR_Msk) >> SPI_CR1_BR_Pos)),
                     freq_buf));
    }
#endif
    if (all || RCC->APB1ENR & RCC_APB1ENR_USART2EN) {
        streamer_printf(streamer, "    USART2 %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_USART2EN));
    }
#ifdef RCC_APB1ENR_USART3EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_USART3EN) {
        streamer_printf(streamer, "    USART3 %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_USART3EN));
    }
#endif
#ifdef RCC_APB1ENR_UART4EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_UART4EN) {
        streamer_printf(streamer, "    UART4  %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_UART4EN));
    }
#endif
#ifdef RCC_APB1ENR_UART5EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_UART5EN) {
        streamer_printf(streamer, "    UART5  %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_UART5EN));
    }
#endif
    if (all || RCC->APB1ENR & RCC_APB1ENR_I2C1EN) {
        streamer_printf(streamer, "    I2C1   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_I2C1EN));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_I2C2EN) {
        streamer_printf(streamer, "    I2C2   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_I2C2EN));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_I2C3EN) {
        streamer_printf(streamer, "    I2C3   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_I2C3EN));
    }
#ifdef RCC_APB1ENR_CAN1EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_CAN1EN) {
        streamer_printf(streamer, "    CAN1   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_CAN1EN));
    }
#endif
#ifdef RCC_APB1ENR_CAN2EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_CAN2EN) {
        streamer_printf(streamer, "    CAN2   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_CAN2EN));
    }
#endif
    if (all || RCC->APB1ENR & RCC_APB1ENR_PWREN) {
        streamer_printf(streamer, "    PWR    %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_PWREN));
    }
#ifdef RCC_APB1ENR_DACEN
    if (all || RCC->APB1ENR & RCC_APB1ENR_DACEN) {
        streamer_printf(streamer, "    DAC    %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_DACEN));
    }
#endif
}

static void
print_apb2_peripherals(struct streamer *streamer, bool all)
{
    uint32_t pckl2 = HAL_RCC_GetPCLK2Freq();
    uint32_t adcpre = (((ADC->CCR & ADC_CCR_ADCPRE_Msk) >> ADC_CCR_ADCPRE_Pos) + 1) * 2;
    uint32_t timmul = RCC->CFGR & RCC_CFGR_PPRE2_2 ? 2 : 1;
    char freq_buf[20];

    streamer_printf(streamer, "  APB2 PCLK2: %s\n", freq_str(pckl2, freq_buf));

    if (all || RCC->APB2ENR & RCC_APB2ENR_USART1EN) {
        streamer_printf(streamer, "    USART1 %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_USART1EN));
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_USART6EN) {
        streamer_printf(streamer, "    USART6 %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_USART6EN));
    }

    if (all || RCC->APB2ENR & RCC_APB2ENR_ADC1EN) {
        streamer_printf(streamer, "    ADC1   %s %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_ADC1EN),
                        freq_str(pckl2 / adcpre, freq_buf));
    }
#ifdef RCC_APB2ENR_ADC2EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_ADC2EN) {
        streamer_printf(streamer, "    ADC2   %s %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_ADC2EN),
                        freq_str(pckl2 / adcpre, freq_buf));
    }
#endif
#if defined(RCC_APB2ENR_ADC3EN)
    if (all || RCC->APB2ENR & RCC_APB2ENR_ADC3EN) {
        streamer_printf(streamer, "    ADC3   %s %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_ADC3EN),
                        freq_str(pckl2 / adcpre, freq_buf));
    }
#endif
    if (all || RCC->APB2ENR & RCC_APB2ENR_SDIOEN) {
        streamer_printf(streamer, "    SDIO   %s %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_SDIOEN),
                        freq_str(stm32f4xx_pll_q_freq() /
                                     (2 + (SDIO->CLKCR & SDIO_CLKCR_CLKDIV_Msk)),
                                 freq_buf));
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_SPI1EN) {
        streamer_printf(
            streamer, "    SPI1   %s %s\n",
            on_off_state(RCC->APB2ENR & RCC_APB2ENR_SPI1EN),
            freq_str(pckl2 >> (1 + ((SPI1->CR1 & SPI_CR1_BR_Msk) >> SPI_CR1_BR_Pos)),
                     freq_buf));
    }
#if defined(RCC_APB2ENR_SPI4EN)
    if (all || RCC->APB2ENR & RCC_APB2ENR_SPI4EN) {
        streamer_printf(
            streamer, "    SPI4   %s %s%s\n",
            on_off_state(RCC->APB2ENR & RCC_APB2ENR_SPI4EN),
            freq_str(pckl2 >> (1 + ((SPI4->CR1 & SPI_CR1_BR_Msk) >> SPI_CR1_BR_Pos)),
                     freq_buf),
            (SPI4->I2SCFGR & SPI_I2SCFGR_I2SMOD) ? " (I2S)" : "");
    }
#endif
#if defined(RCC_APB2ENR_SPI5EN)
    if (all || RCC->APB2ENR & RCC_APB2ENR_SPI5EN) {
        streamer_printf(
            streamer, "    SPI5   %s %s%s\n",
            on_off_state(RCC->APB2ENR & RCC_APB2ENR_SPI5EN),
            freq_str(pckl2 >> (1 + ((SPI5->CR1 & SPI_CR1_BR_Msk) >> SPI_CR1_BR_Pos)),
                     freq_buf),
            (SPI5->I2SCFGR & SPI_I2SCFGR_I2SMOD) ? " (I2S)" : "");
    }
#endif
    if (all || RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN) {
        streamer_printf(streamer, "    SYSCFG %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN));
    }

    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM1EN) {
        streamer_printf(streamer, "    TIM1   %s %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_TIM1EN),
                        freq_str(pckl2 * timmul / (TIM1->PSC + 1), freq_buf));
    }
#ifdef RCC_APB2ENR_TIM8EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM8EN) {
        streamer_printf(streamer, "    TIM8   %s %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_TIM8EN),
                        freq_str(pckl2 * timmul / (TIM8->PSC + 1), freq_buf));
    }
#endif
#ifdef RCC_APB2ENR_TIM9EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM9EN) {
        streamer_printf(streamer, "    TIM9   %s %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_TIM9EN),
                        freq_str(pckl2 * timmul / (TIM9->PSC + 1), freq_buf));
    }
#endif
#ifdef RCC_APB2ENR_TIM10EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM10EN) {
        streamer_printf(streamer, "    TIM10  %s %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_TIM10EN),
                        freq_str(pckl2 * timmul / (TIM10->PSC + 1), freq_buf));
    }
#endif
#ifdef RCC_APB2ENR_TIM11EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM11EN) {
        streamer_printf(streamer, "    TIM11  %s %s\n",
                        on_off_state(RCC->APB2ENR & RCC_APB2ENR_TIM11EN),
                        freq_str(pckl2 * timmul / (TIM11->PSC + 1), freq_buf));
    }
#endif
}

static int
mcu_cli_info_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                 struct streamer *streamer)
{
    bool all;
    char freq_buf[20];
    int sw = ((RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos);

    all = argc > 1 && strcmp(argv[1], "all") == 0;

    streamer_printf(streamer, "Clocks:\n");
    streamer_printf(streamer, "  SYSCLK: %s\n", freq_str(SystemCoreClock, freq_buf));
    streamer_printf(streamer, "    source %s\n", system_clock_source[sw]);
    streamer_printf(streamer, "  HSI: %s\n", on_off_state(RCC->CR & RCC_CR_HSION));
    streamer_printf(streamer, "  HSE: %s\n", on_off_state(RCC->CR & RCC_CR_HSEON));
    streamer_printf(streamer, "  PLL: %s\n", on_off_state(RCC->CR & RCC_CR_PLLON));
    if (RCC->CR & RCC_CR_PLLON) {
        streamer_printf(streamer, "     PLLP: %s\n",
                        freq_str(stm32f4xx_pll_p_freq(), freq_buf));
        streamer_printf(streamer, "     PLLQ: %s\n",
                        freq_str(stm32f4xx_pll_q_freq(), freq_buf));
#ifdef RCC_PLLCFGR_PLLR
        streamer_printf(streamer, "     PLLR: %s\n",
                        freq_str(stm32f4xx_pll_q_freq(), freq_buf));
#endif
    }
    streamer_printf(streamer, "  LSI: %s\n", on_off_state(RCC->CSR & RCC_CSR_LSION));
    streamer_printf(streamer, "  LSE: %s\n", on_off_state(RCC->BDCR & RCC_BDCR_LSEON));
    streamer_printf(streamer, "Peripherals:\n");
    print_ahb_peripherals(streamer, all);
    print_apb1_peripherals(streamer, all);
    print_apb2_peripherals(streamer, all);

    return 0;
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

static const struct shell_cmd mcu_cli_commands[] = {
    SHELL_CMD_EXT("info", mcu_cli_info_cmd, &mcu_cli_info_help),
};

SHELL_MODULE_WITH_TABLE(mcu, mcu_cli_commands)
