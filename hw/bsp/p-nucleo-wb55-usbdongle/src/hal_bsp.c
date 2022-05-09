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

#include <stm32wb55xx.h>
#include <stm32_common/stm32_hal.h>

#if MYNEWT_VAL(ETH_0)
#include <stm32_eth/stm32_eth.h>
#include <stm32_eth/stm32_eth_cfg.h>
#endif

#if MYNEWT_VAL(PWM_0) || MYNEWT_VAL(PWM_1) || MYNEWT_VAL(PWM_2)
#include <pwm_stm32/pwm_stm32.h>
#endif

#if MYNEWT_VAL(UART_0)
const struct stm32_uart_cfg os_bsp_uart0_cfg = {
    .suc_uart = USART1,
    .suc_rcc_reg = &RCC->APB2ENR,
    .suc_rcc_dev = RCC_APB2ENR_USART1EN,
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
    .suc_pin_af = GPIO_AF7_USART1,
    .suc_irqn = USART1_IRQn,
};
#endif

#if MYNEWT_VAL(I2C_0)
const struct stm32_hal_i2c_cfg os_bsp_i2c0_cfg = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR1,
    .hic_rcc_dev = RCC_APB1ENR1_I2C1EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_timingr = 0x00C0216C,            /* 400KHz at 64MHz */
};
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE,
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

#if MYNEWT_VAL(BUTTON_1_AS_STM32_DFU)
void
boot_preboot(void)
{
    hal_gpio_init_in(BUTTON_1, HAL_GPIO_PULL_UP);
    os_cputime_delay_usecs(100);
    if (hal_gpio_read(BUTTON_1) == 0) {
        hal_gpio_deinit(BUTTON_1);
        stm32_start_bootloader();
    }
    hal_gpio_deinit(BUTTON_1);
}
#endif

static void
dongle_soft_reset(void *arg)
{
    os_cputime_delay_usecs(800000);
    hal_system_reset();
}

void
hal_bsp_init(void)
{
    stm32_periph_create();

    if (MYNEWT_VAL(BUTTON_1_AS_RESET)) {
        hal_gpio_irq_init(BUTTON_1, dongle_soft_reset, NULL, HAL_GPIO_TRIG_FALLING, HAL_GPIO_PULL_UP);
    }
}

void
hal_bsp_deinit(void)
{
    RCC->AHB1ENR = 0;
    RCC->AHB2ENR = 0;
    RCC->AHB3ENR = RCC_AHB3ENR_FLASHEN | RCC_AHB3ENR_HSEMEN;
    RCC->APB1ENR1 = RCC_APB1ENR1_RTCAPBEN;
    RCC->APB1ENR2 = 0;
    RCC->APB2ENR = 0;
    RCC->AHB1RSTR = 0x00011007;
    RCC->AHB2RSTR = 0x0001209F;
    RCC->AHB3RSTR = 0x021F1000;
    RCC->APB1RSTR1 = 0x85A04201;
    RCC->APB1RSTR2 = 0x00000021;
    RCC->APB2RSTR = 0x00265800;

    RCC->AHB1RSTR = 0;
    RCC->AHB2RSTR = 0;
    RCC->AHB3RSTR = 0;
    RCC->APB1RSTR1 = 0;
    RCC->APB1RSTR2 = 0;
    RCC->APB2RSTR = 0;
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
