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
#include "mynewt_cm.h"

#include <hal/hal_bsp.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_system.h>

#include <stm32g4xx.h>
#include <stm32_common/stm32_hal.h>
#if MYNEWT_PKG_apache_mynewt_core__bus_drivers
#include <bus/drivers/spi_common.h>
#endif

#if MYNEWT_VAL(PWM_0) || MYNEWT_VAL(PWM_1) || MYNEWT_VAL(PWM_2)
#include <pwm_stm32/pwm_stm32.h>
#endif

#if MYNEWT_VAL(PWM_0)
struct stm32_pwm_conf os_bsp_pwm0_cfg = {
    .tim = TIM3,
    .irq = TIM3_IRQn,
};
#endif
#if MYNEWT_VAL(PWM_1)
struct stm32_pwm_conf os_bsp_pwm1_cfg = {
    .tim = TIM4,
    .irq = TIM4_IRQn,
};
#endif
#if MYNEWT_VAL(PWM_2)
struct stm32_pwm_conf os_bsp_pwm2_cfg = {
    .tim = TIM1,
    .irq = TIM1_CC_IRQn,
};
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

#if MYNEWT_VAL(UART_1)
const struct stm32_uart_cfg os_bsp_uart1_cfg = {
    .suc_uart = USART2,
    .suc_rcc_reg = &RCC->APB1ENR1,
    .suc_rcc_dev = RCC_APB1ENR1_USART2EN,
    .suc_pin_tx = MYNEWT_VAL(UART_1_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_1_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_1_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_1_PIN_CTS),
    .suc_pin_af = GPIO_AF7_USART2,
    .suc_irqn = USART2_IRQn,
};
#endif

#if MYNEWT_VAL(I2C_0)
/*
 * The PB8 and PB9 pins are connected through jumpers in the board to
 * both ADC_IN and I2C pins. To enable I2C functionality SB147/SB157 need
 * to be removed (they are the default connections) and SB138/SB143 need
 * to be shorted.
 */
const struct stm32_hal_i2c_cfg os_bsp_i2c0_cfg = {
    .hic_i2c = I2C1,
    .hic_rcc_reg = &RCC->APB1ENR1,
    .hic_rcc_dev = RCC_APB1ENR1_I2C1EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_speed = 100000,                    /* 100kHz */
};
#endif

#if MYNEWT_VAL(I2C_1)
const struct stm32_hal_i2c_cfg os_bsp_i2c1_cfg = {
    .hic_i2c = I2C2,
    .hic_rcc_reg = &RCC->APB1ENR1,
    .hic_rcc_dev = RCC_APB1ENR1_I2C2EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_1_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_1_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C2,
    .hic_10bit = 0,
    .hic_speed = 100000,                    /* 100kHz */
};
#endif

#if MYNEWT_VAL(I2C_2)
const struct stm32_hal_i2c_cfg os_bsp_i2c2_cfg = {
    .hic_i2c = I2C3,
    .hic_rcc_reg = &RCC->APB1ENR1,
    .hic_rcc_dev = RCC_APB1ENR1_I2C3EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_2_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_2_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C3,
    .hic_10bit = 0,
    .hic_speed = 100000,                    /* 100kHz */
};
#endif

static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE,
    },
};

extern const struct hal_flash stm32_flash_dev;

static const struct hal_flash *flash_devs[] = {
    [0] = &stm32_flash_dev,
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    if (id >= ARRAY_SIZE(flash_devs)) {
        return NULL;
    }

    return flash_devs[id];
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

void
hal_bsp_deinit(void)
{
    Cortex_DisableAll();

    RCC->AHB1ENR = 0x00000100;
    RCC->AHB2ENR = 0x00000000;
    RCC->AHB3ENR = 0x80000000;
    RCC->APB1ENR1 = 0x00000400;
    RCC->APB1ENR2 = 0;
    RCC->APB2ENR = 0;

    RCC->AHB1RSTR = 0x0000111F;
    RCC->AHB2RSTR = 0x050F607F;
    RCC->AHB3RSTR = 0x00000101;
    RCC->APB1RSTR1 = 0xD2FECD2F;
    RCC->APB1RSTR2 = 0x00800103;
    RCC->APB2RSTR = 0x0437F801;
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
