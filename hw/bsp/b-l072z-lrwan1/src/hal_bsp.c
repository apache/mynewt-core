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

#include <stm32l072xx.h>
#include <stm32_common/stm32_hal.h>

#if MYNEWT_VAL(PWM_0) || MYNEWT_VAL(PWM_1) || MYNEWT_VAL(PWM_2)
#include <pwm_stm32/pwm_stm32.h>
#endif

#if MYNEWT_VAL(LORA_NODE)

/*
 * Sanity checks for when LoRaWAN is enabled.
 */

#if   ((MYNEWT_VAL(LORA_MAC_TIMER_NUM) == 0) && !MYNEWT_VAL(TIMER_0))
#error "TIMER_0 is used by LoRa and has to be enabled"
#elif ((MYNEWT_VAL(LORA_MAC_TIMER_NUM) == 1) && !MYNEWT_VAL(TIMER_1))
#error "TIMER_1 is used by LoRa and has to be enabled"
#elif ((MYNEWT_VAL(LORA_MAC_TIMER_NUM) == 2) && !MYNEWT_VAL(TIMER_2))
#error "TIMER_2 is used by LoRa and has to be enabled"
#endif

#if   ((MYNEWT_VAL(SX1276_SPI_IDX) == 0) && !MYNEWT_VAL(SPI_0_MASTER))
#error "SPI_0_MASTER is used by LoRa and has to be enabled"
#elif ((MYNEWT_VAL(SX1276_SPI_IDX) == 1) && !MYNEWT_VAL(SPI_1_MASTER))
#error "SPI_1_MASTER is used by LoRa and has to be enabled"
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) < 0)
#error "OS_CPUTIME_TIMER_NUM is used by LoRa and has to be enabled"
#endif

#endif /* MYNEWT_VAL(LORA_NODE) */

#if MYNEWT_VAL(UART_0)
const struct stm32_uart_cfg os_bsp_uart0_cfg = {
    .suc_uart = USART2,
    .suc_rcc_reg = &RCC->APB1ENR,
    .suc_rcc_dev = RCC_APB1ENR_USART2EN,
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
    .suc_pin_af = GPIO_AF4_USART2,
    .suc_irqn = USART2_IRQn,
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
    .hic_timingr = 0x10420F13,      /* FIXME: 100KHz at 8MHz of SysCoreClock */
};
#endif

#if ((MYNEWT_VAL(SPI_1_SLAVE) || MYNEWT_VAL(SPI_1_MASTER)) && \
    MYNEWT_VAL(SPI_1_CUSTOM_CFG))
const struct stm32_hal_spi_cfg os_bsp_spi1_cfg = {
#if (MYNEWT_VAL(LORA_NODE) && (MYNEWT_VAL(SX1276_SPI_IDX) == 1))
    .ss_pin = MYNEWT_VAL(SX1276_SPI_CS_PIN),
#else
    .ss_pin = MYNEWT_VAL(SPI_1_PIN_SS),
#endif
    .sck_pin = MYNEWT_VAL(SPI_1_PIN_SCK),
    .miso_pin = MYNEWT_VAL(SPI_1_PIN_MISO),
    .mosi_pin = MYNEWT_VAL(SPI_1_PIN_MOSI),
    .irq_prio = 2,
};
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    },
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

void
hal_bsp_init(void)
{
    stm32_periph_create();
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

#if MYNEWT_VAL(LORA_NODE)
void lora_bsp_enable_mac_timer(void)
{
    /* Turn on the LoRa MAC timer. This function is automatically
     * called by the LoRa stack when exiting low power mode.*/
    #if MYNEWT_VAL(LORA_MAC_TIMER_NUM) == 0
        #define LORA_TIM  MYNEWT_VAL(TIMER_0_TIM)
    #elif MYNEWT_VAL(LORA_MAC_TIMER_NUM) == 1
        #define LORA_TIM  MYNEWT_VAL(TIMER_1_TIM)
    #elif MYNEWT_VAL(LORA_MAC_TIMER_NUM) == 2
        #define LORA_TIM  MYNEWT_VAL(TIMER_2_TIM)
    #else
        #error "Invalid LORA_MAC_TIMER_NUM"
    #endif

    hal_timer_init(MYNEWT_VAL(LORA_MAC_TIMER_NUM), LORA_TIM);
}
#endif
