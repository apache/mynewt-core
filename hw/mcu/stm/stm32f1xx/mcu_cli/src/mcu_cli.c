/*
 * Copyright 2020 Jesus Ipanienko
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "os/mynewt.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stm32f1xx_hal.h>
#include "shell/shell.h"
#include "console/console.h"

extern uint32_t SystemCoreClock;

static const char *system_clock_source[4] = { "HSI", "HSE", "PLL", "" };

static const char *
on_off_state(uint32_t on)
{
    return on ? "on" : "off";
}

static void
print_ahb_peripherals(struct streamer *streamer, bool all)
{
    streamer_printf(streamer, "  AHB HCLK: %u\n", (unsigned int)HAL_RCC_GetHCLKFreq());

    if (all || RCC->AHBENR & RCC_AHBENR_DMA1EN) {
        streamer_printf(streamer, "    DMA1   %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_DMA1EN));
    }
#ifdef RCC_AHBENR_DMA2EN
    if (all || RCC->AHBENR & RCC_AHBENR_DMA2EN) {
        streamer_printf(streamer, "    DMA2   %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_DMA2EN));
    }
#endif
    if (all || RCC->AHBENR & RCC_AHBENR_SRAMEN) {
        streamer_printf(streamer, "    SRAM   %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_SRAMEN));
    }
#ifdef RCC_AHBENR_FLITFEN
    if (all || RCC->AHBENR & RCC_AHBENR_FLITFEN) {
        streamer_printf(streamer, "    FLITF  %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_FLITFEN));
    }
#endif
#ifdef RCC_AHBENR_FSMCEN
    if (all || RCC->AHBENR & RCC_AHBENR_FSMCEN) {
        streamer_printf(streamer, "    TSC    %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_FSMCEN));
    }
#endif
#ifdef RCC_AHBENR_SDIOEN
    if (all || RCC->AHBENR & RCC_AHBENR_SDIOEN) {
        streamer_printf(streamer, "    SDIO   %s\n", on_off_state(RCC->AHBENR & RCC_AHBENR_SDIOEN));
    }
#endif
}

static void
print_apb1_peripherals(struct streamer *streamer, bool all)
{
    streamer_printf(streamer, "  APB1 PCLK1: %u\n", (unsigned int)HAL_RCC_GetPCLK1Freq());
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM2EN) {
        streamer_printf(streamer, "    TIM2   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM2EN));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM3EN) {
        streamer_printf(streamer, "    TIM3   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM3EN));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM4EN) {
        streamer_printf(streamer, "    TIM4   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM4EN));
    }
#ifdef RCC_APB1ENR_TIM5EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM5EN) {
        streamer_printf(streamer, "    TIM5   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM5EN));
    }
#endif
#ifdef RCC_APB1ENR_TIM6EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM6EN) {
        streamer_printf(streamer, "    TIM6   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM6EN));
    }
#endif
#ifdef RCC_APB1ENR_TIM7EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM7EN) {
        streamer_printf(streamer, "    TIM7   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM7EN));
    }
#endif
#ifdef RCC_APB1ENR_TIM12EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM12EN) {
        streamer_printf(streamer, "    TIM12  %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM12EN));
    }
#endif
#ifdef RCC_APB1ENR_TIM13EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM13EN) {
        streamer_printf(streamer, "    TIM13  %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM13EN));
    }
#endif
#ifdef RCC_APB1ENR_TIM14EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_TIM14EN) {
        streamer_printf(streamer, "    TIM14  %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_TIM14EN));
    }
#endif
    if (all || RCC->APB1ENR & RCC_APB1ENR_WWDGEN) {
        streamer_printf(streamer, "    WWD    %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_WWDGEN));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_SPI2EN) {
        streamer_printf(streamer, "    SPI2   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_SPI2EN));
    }
#ifdef RCC_APB1ENR_SPI3EN
    if (all || RCC->APB1ENR & RCC_APB1ENR_SPI3EN) {
        streamer_printf(streamer, "    SPI3   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_SPI3EN));
    }
#endif
    if (all || RCC->APB1ENR & RCC_APB1ENR_USART2EN) {
        streamer_printf(streamer, "    USART2 %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_USART2EN));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_USART3EN) {
        streamer_printf(streamer, "    USART3 %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_USART3EN));
    }
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
    if (all || RCC->APB1ENR & RCC_APB1ENR_CAN1EN) {
        streamer_printf(streamer, "    CAN1   %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_CAN1EN));
    }
    if (all || RCC->APB1ENR & RCC_APB1ENR_BKPEN) {
        streamer_printf(streamer, "    BKP    %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_BKPEN));
    }
#ifdef RCC_APB1ENR_USBEN
    if (all || RCC->APB1ENR & RCC_APB1ENR_USBEN) {
        streamer_printf(streamer, "    USB    %s\n", on_off_state(RCC->APB1ENR & RCC_APB1ENR_USBEN));
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
    streamer_printf(streamer, "  APB2 PCLK2: %u\n", (unsigned int)HAL_RCC_GetPCLK2Freq());
    if (all || RCC->APB2ENR & RCC_APB2ENR_AFIOEN) {
        streamer_printf(streamer, "    AFIO   %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_AFIOEN));
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_IOPAEN) {
        streamer_printf(streamer, "    IOA    %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_IOPAEN));
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_IOPBEN) {
        streamer_printf(streamer, "    IOB    %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_IOPBEN));
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_IOPCEN) {
        streamer_printf(streamer, "    IOC    %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_IOPCEN));
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_IOPDEN) {
        streamer_printf(streamer, "    IOD    %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_IOPDEN));
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_IOPEEN) {
        streamer_printf(streamer, "    IOE    %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_IOPEEN));
    }
#ifdef RCC_APB2ENR_IOPFEN
    if (all || RCC->APB2ENR & RCC_APB2ENR_IOPFEN) {
        streamer_printf(streamer, "    IOF    %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_IOPFEN));
    }
#endif
#ifdef RCC_APB2ENR_IOPGEN
    if (all || RCC->APB2ENR & RCC_APB2ENR_IOPGEN) {
        streamer_printf(streamer, "    IOG    %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_IOPGEN));
    }
#endif

    if (all || RCC->APB2ENR & RCC_APB2ENR_ADC1EN) {
        streamer_printf(streamer, "    ADC1   %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_ADC1EN));
    }
    if (all || RCC->APB2ENR & RCC_APB2ENR_ADC2EN) {
        streamer_printf(streamer, "    ADC2   %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_ADC2EN));
    }
#ifdef RCC_APB2ENR_ADC3EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_ADC3EN) {
        streamer_printf(streamer, "    ADC3   %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_ADC3EN));
    }
#endif

    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM1EN) {
        streamer_printf(streamer, "    TIM1   %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_TIM1EN));
    }
#ifdef RCC_APB2ENR_TIM8EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM8EN) {
        streamer_printf(streamer, "    TIM8   %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_TIM8EN));
    }
#endif
#ifdef RCC_APB2ENR_TIM9EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM9EN) {
        streamer_printf(streamer, "    TIM9   %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_TIM9EN));
    }
#endif
#ifdef RCC_APB2ENR_TIM10EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM10EN) {
        streamer_printf(streamer, "    TIM10  %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_TIM10EN));
    }
#endif
#ifdef RCC_APB2ENR_TIM11EN
    if (all || RCC->APB2ENR & RCC_APB2ENR_TIM11EN) {
        streamer_printf(streamer, "    TIM11  %s\n", on_off_state(RCC->APB2ENR & RCC_APB2ENR_TIM11EN));
    }
#endif
}

static int
mcu_cli_info_cmd(const struct shell_cmd *cmd, int argc, char **argv,
                 struct streamer *streamer)
{
    bool all;
    int sw = ((RCC->CFGR & RCC_CFGR_SWS) >> RCC_CFGR_SWS_Pos);

    all = argc > 1 && strcmp(argv[1], "all") == 0;

    streamer_printf(streamer, "Clocks:\n");
    streamer_printf(streamer, "  SYSCLK: %u\n", (unsigned int)SystemCoreClock);
    streamer_printf(streamer, "    source %s\n", system_clock_source[sw]);
    streamer_printf(streamer, "  HSI: %s\n", on_off_state(RCC->CR & RCC_CR_HSION));
    streamer_printf(streamer, "  HSE: %s\n", on_off_state(RCC->CR & RCC_CR_HSEON));
    streamer_printf(streamer, "  PLL: %s\n", on_off_state(RCC->CR & RCC_CR_PLLON));
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
