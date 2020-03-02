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
#include <assert.h>

#include "bsp/bsp.h"
#include "os/mynewt.h"

#include <hal/hal_bsp.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_system.h>

#include "stm32f405xx.h"
#include <stm32_common/stm32_hal.h>

#if MYNEWT_VAL(ETH_0)
#include <stm32_eth/stm32_eth.h>
#include <stm32_eth/stm32_eth_cfg.h>
#endif

#if MYNEWT_VAL(PWM_0) || MYNEWT_VAL(PWM_1) || MYNEWT_VAL(PWM_2)
#include <pwm_stm32/pwm_stm32.h>
#endif

#if MYNEWT_VAL(ADC_0) || MYNEWT_VAL(ADC_1) || MYNEWT_VAL(ADC_2)
#include "stm32f4xx_hal_adc.h"
#include "adc_stm32f4/adc_stm32f4.h"
#endif

const uint32_t stm32_flash_sectors[] = {
    0x08000000,     /* 16kB */
    0x08004000,     /* 16kB */
    0x08008000,     /* 16kB */
    0x0800c000,     /* 16kB */
    0x08010000,     /* 64kB */
    0x08020000,     /* 128kB */
    0x08040000,     /* 128kB */
    0x08060000,     /* 128kB */
    0x08080000,     /* 128kB */
    0x080a0000,     /* 128kB */
    0x080c0000,     /* 128kB */
    0x080e0000,     /* 128kB */
    0x08100000,     /* End of flash */
};

#define SZ (sizeof(stm32_flash_sectors) / sizeof(stm32_flash_sectors[0]))
static_assert(MYNEWT_VAL(STM32_FLASH_NUM_AREAS) + 1 == SZ,
        "STM32_FLASH_NUM_AREAS does not match flash sectors");

#if MYNEWT_VAL(ADC_0) || MYNEWT_VAL(ADC_1) || MYNEWT_VAL(ADC_2)
/*
 * Helper macros to build ADC data structures
 */
#define STM32F4_DEFAULT_DMA_HANDLE(instance, channel, parent) {           \
        .Instance = (instance),                                           \
        .Init.Channel = (channel),                                        \
        .Init.Direction = DMA_PERIPH_TO_MEMORY,                           \
        .Init.PeriphInc = DMA_PINC_DISABLE,                               \
        .Init.MemInc = DMA_MINC_ENABLE,                                   \
        .Init.PeriphDataAlignment = DMA_PDATAALIGN_WORD,                  \
        .Init.MemDataAlignment = DMA_MDATAALIGN_WORD,                     \
        .Init.Mode = DMA_CIRCULAR,                                        \
        .Init.Priority = DMA_PRIORITY_HIGH,                               \
        .Init.FIFOMode = DMA_FIFOMODE_DISABLE,                            \
        .Init.FIFOThreshold = DMA_FIFO_THRESHOLD_HALFFULL,                \
        .Init.MemBurst = DMA_MBURST_SINGLE,                               \
        .Init.PeriphBurst = DMA_PBURST_SINGLE,                            \
        .Parent = (parent),                                               \
}

#define ADC_INIT_HANDLE(name, instance, dma_handle)                       \
    ADC_HandleTypeDef (name) = {                                          \
        .Init = {                                                         \
            .ClockPrescaler = ADC_CLOCKPRESCALER_PCLK_DIV2,               \
            .Resolution = ADC_RESOLUTION12b,                              \
            .DataAlign = ADC_DATAALIGN_RIGHT,                             \
            .ScanConvMode = DISABLE,                                      \
            .EOCSelection = DISABLE,                                      \
            .ContinuousConvMode = ENABLE,                                 \
            .NbrOfConversion = 1,                                         \
            .DiscontinuousConvMode = DISABLE,                             \
            .NbrOfDiscConversion = 0,                                     \
            .ExternalTrigConv = ADC_SOFTWARE_START,                       \
            .ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE,        \
            .DMAContinuousRequests = ENABLE,                              \
        },                                                                \
        .Instance = (instance),                                           \
        .NbrOfCurrentConversionRank = 0,                                  \
        .DMA_Handle = (dma_handle),                                       \
        .Lock = HAL_UNLOCKED,                                             \
        .State = 0,                                                       \
        .ErrorCode = 0,                                                   \
    }
#endif

#if MYNEWT_VAL(ADC_0)
ADC_HandleTypeDef adc0_handle;

DMA_HandleTypeDef adc0_dma00_handle = STM32F4_DEFAULT_DMA_HANDLE(
    DMA2_Stream0, DMA_CHANNEL_0, &adc0_handle);

ADC_INIT_HANDLE(adc0_handle, ADC1, &adc0_dma00_handle);

struct stm32f4_adc_dev_cfg os_bsp_adc0_cfg = {
    .sac_chan_count = 16,
    .sac_chans = (struct adc_chan_config [16]) {
        [0] = { 0 },
        [1] = { 0 },
        [2] = { 0 },
        [3] = { 0 },
        [4] = { 0 },
        [5] = { 0 },
        [6] = { 0 },
        [7] = { 0 },
        [8] = { 0 },
        [9] = { 0 },
        [10] = {
            .c_refmv = 3300,
            .c_res = 12,
            .c_configured = 1,
            .c_cnum = ADC_CHANNEL_10,
        },
    },
    .sac_adc_handle = &adc0_handle,
};
#endif

#if MYNEWT_VAL(ADC_1)
ADC_HandleTypeDef adc1_handle;

DMA_HandleTypeDef adc1_dma21_handle = STM32F4_DEFAULT_DMA_HANDLE(
    DMA2_Stream2, DMA_CHANNEL_1, &adc1_handle);

ADC_INIT_HANDLE(adc1_handle, ADC2, &adc1_dma21_handle);

struct stm32f4_adc_dev_cfg os_bsp_adc1_cfg = {
    .sac_chan_count = 16,
    .sac_chans = (struct adc_chan_config [16]) {
        [0] = { 0 },
        [1] = {
            .c_refmv = 3300,
            .c_res = 12,
            .c_configured = 1,
            .c_cnum = ADC_CHANNEL_1,
        },
    },
    .sac_adc_handle = &adc1_handle,
};
#endif

#if MYNEWT_VAL(ADC_2)
ADC_HandleTypeDef adc2_handle;

DMA_HandleTypeDef adc2_dma12_handle = STM32F4_DEFAULT_DMA_HANDLE(
    DMA2_Stream1, DMA_CHANNEL_2, &adc2_handle);

ADC_INIT_HANDLE(adc2_handle, ADC3, &adc2_dma12_handle);

struct stm32f4_adc_dev_cfg os_bsp_adc2_cfg = {
    .sac_chan_count = 16,
    .sac_chans = (struct adc_chan_config [16]) {
        [0] = { 0 },
        [1] = { 0 },
        [2] = { 0 },
        [3] = { 0 },
        [4] = {
            .c_refmv = 3300,
            .c_res = 12,
            .c_configured = 1,
            .c_cnum = ADC_CHANNEL_4,
        },
    },
    .sac_adc_handle = &adc2_handle,
};
#endif

#if MYNEWT_VAL(UART_0)
const struct stm32_uart_cfg os_bsp_uart0_cfg = {
    .suc_uart = USART3,
    .suc_rcc_reg = &RCC->APB1ENR,
    .suc_rcc_dev = RCC_APB1ENR_USART3EN,
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
    .suc_pin_af = GPIO_AF7_USART3,
    .suc_irqn = USART3_IRQn
};
#endif

#if MYNEWT_VAL(I2C_0)
const struct stm32_hal_i2c_cfg os_bsp_i2c0_cfg = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR,
    .hic_rcc_dev = RCC_APB1ENR_I2C1EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_speed = 100000,
};
#endif

#if MYNEWT_VAL(ETH_0)
const struct stm32_eth_cfg os_bsp_eth0_cfg = {
    /*
     * PORTA
     *   PA1 - ETH_RMII_REF_CLK
     *   PA2 - ETH_RMII_MDIO
     *   PA3 - ETH_RMII_MDINT  (GPIO irq?)
     *   PA7 - ETH_RMII_CRS_DV
     */
    .sec_port_mask[0] = (1 << 1) | (1 << 2) | (1 << 7),

    /*
     * PORTC
     *   PC1 - ETH_RMII_MDC
     *   PC4 - ETH_RMII_RXD0
     *   PC5 - ETH_RMII_RXD1
     */
    .sec_port_mask[2] = (1 << 1) | (1 << 4) | (1 << 5),

    /*
     * PORTG
     *   PG11 - ETH_RMII_TXEN
     *   PG13 - ETH_RMII_TXD0
     *   PG14 - ETH_RMII_TXD1
     */
    .sec_port_mask[6] = (1 << 11) | (1 << 13) | (1 << 14),
    .sec_phy_type = SMSC_8710_RMII,
    .sec_phy_irq = MCU_GPIO_PORTA(3)
};
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    },
    [1] = {
        .hbmd_start = &_ccram_start,
        .hbmd_size = CCRAM_SIZE
    }
};

extern const struct hal_flash stm32_flash_dev;
const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }
    return &stm32_flash_dev;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

int
hal_bsp_power_state(int state)
{
    return (0);
}

/**
 * Returns the configured priority for the given interrupt. If no priority
 * configured, return the priority passed in
 *
 * @param irq_num
 * @param pri
 *
 * @return uint32_t
 */
uint32_t
hal_bsp_get_nvic_priority(int irq_num, uint32_t pri)
{
    /* Add any interrupt priorities configured by the bsp here */
    return pri;
}

void
hal_bsp_init(void)
{
    stm32_periph_create();
}
