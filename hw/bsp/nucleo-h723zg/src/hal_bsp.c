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
#include "spiflash/spiflash.h"

#include <hal/hal_bsp.h>
#include <hal/hal_flash_int.h>
#include <hal/hal_system.h>

#include <stm32h723xx.h>
#include <stm32_common/stm32_hal.h>
#include <bus/drivers/spi_common.h>

#if MYNEWT_VAL(ETH_0)
#include <stm32_eth/stm32_eth.h>
#include <stm32_eth/stm32_eth_cfg.h>
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
    .tim = TIM12,
    .irq = TIM8_BRK_TIM12_IRQn,
};
#endif

#if MYNEWT_VAL(UART_0)
const struct stm32_uart_cfg os_bsp_uart0_cfg = {
    .suc_uart = USART3,
    .suc_rcc_reg = &RCC->APB1LENR,
    .suc_rcc_dev = RCC_APB1LENR_USART3EN,
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
    .suc_pin_af = GPIO_AF7_USART3,
    .suc_irqn = USART3_IRQn,
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
    .hic_rcc_reg = &RCC->APB1LENR,
    .hic_rcc_dev = RCC_APB1LENR_I2C1EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_0_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_0_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C1,
    .hic_10bit = 0,
    .hic_timingr = 0xA0303048,  /* 100kHz at 544 MHz system clock */
};
#endif

#if MYNEWT_VAL(I2C_1)
const struct stm32_hal_i2c_cfg os_bsp_i2c1_cfg = {
    .hic_i2c = I2C2,
    .hic_rcc_reg = &RCC->APB1LENR,
    .hic_rcc_dev = RCC_APB1LENR_I2C2EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_1_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_1_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C2,
    .hic_10bit = 0,
    .hic_timingr = 0xA0303048,  /* 100kHz at 544 MHz system clock */
};
#endif

#if MYNEWT_VAL(I2C_2)
const struct stm32_hal_i2c_cfg os_bsp_i2c2_cfg = {
    .hic_i2c = I2C3,
    .hic_rcc_reg = &RCC->APB1LENR,
    .hic_rcc_dev = RCC_APB1LENR_I2C3EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_2_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_2_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C3,
    .hic_10bit = 0,
    .hic_timingr = 0xA0303048,  /* 100kHz at 544 MHz system clock */
};
#endif

#if MYNEWT_VAL(I2C_3)
const struct stm32_hal_i2c_cfg os_bsp_i2c3_cfg = {
    .hic_i2c = I2C4,
    .hic_rcc_reg = &RCC->APB4ENR,
    .hic_rcc_dev = RCC_APB4ENR_I2C4EN,
    .hic_pin_sda = MYNEWT_VAL(I2C_3_PIN_SDA),
    .hic_pin_scl = MYNEWT_VAL(I2C_3_PIN_SCL),
    .hic_pin_af = GPIO_AF4_I2C4,
    .hic_10bit = 0,
    .hic_timingr = 0xA0303048,  /* 100kHz at 544 MHz system clock */
};
#endif

#if MYNEWT_VAL(ETH_0)
const struct stm32_eth_cfg os_bsp_eth0_cfg = {
    /*
     * PORTA
     *   PA1 - ETH_RMII_REF_CLK
     *   PA2 - ETH_RMII_MDIO
     *   PA7 - ETH_RMII_CRS_DV
     */
    .sec_port_mask[0] = (1 << 1) | (1 << 2) | (1 << 7),

    /*
     * PORTB
     *   PB13 - ETH_RMII_TXD1
     */
    .sec_port_mask[1] = (1 << 13),

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
     */
    .sec_port_mask[6] = (1 << 11) | (1 << 13),
    .sec_phy_type = LAN_8742_RMII,
    .sec_phy_irq = -1
};
#endif

/* FIXME */
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE,
    },
    [1] = {
        .hbmd_start = &_dtcmram_start,
        .hbmd_size = DTCMRAM_SIZE,
    },
    [2] = {
        .hbmd_start = &_itcmram_start,
        .hbmd_size = ITCMRAM_SIZE,
    },
};

extern const struct hal_flash stm32_flash_dev;

#if MYNEWT_VAL(SPIFLASH)
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
struct bus_spi_node_cfg flash_spi_cfg = {
    .node_cfg.bus_name = MYNEWT_VAL(BSP_FLASH_SPI_BUS),
    .pin_cs = MYNEWT_VAL(SPIFLASH_SPI_CS_PIN),
    .mode = MYNEWT_VAL(SPIFLASH_SPI_MODE),
    .data_order = HAL_SPI_MSB_FIRST,
    .freq = MYNEWT_VAL(SPIFLASH_BAUDRATE),
};
#endif
#endif

static const struct hal_flash *flash_devs[] = {
    [0] = &stm32_flash_dev,
#if MYNEWT_VAL(SPIFLASH)
    [1] = &spiflash_dev.hal,
#endif
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
    int rc;
    (void)rc;

    stm32_periph_create();

#if MYNEWT_VAL(SPIFLASH) && MYNEWT_VAL(BUS_DRIVER_PRESENT)
    rc = spiflash_create_spi_dev(&spiflash_dev.dev,
                                 MYNEWT_VAL(BSP_FLASH_SPI_NAME), &flash_spi_cfg);
    assert(rc == 0);
#endif
}

void
hal_bsp_deinit(void)
{
    Cortex_DisableAll();

    RCC->AHB1ENR = 0;
    RCC->AHB2ENR = 0;
    RCC->AHB3ENR = 0;
    RCC->AHB4ENR = 0;

    RCC->APB1LENR = 0;
    RCC->APB1HENR = 0;
    RCC->APB2ENR = 0;
    RCC->APB3ENR = 0;
    RCC->APB4ENR = 0x0001000;

    RCC->AHB1RSTR = 0x06038203;
    RCC->AHB2RSTR = 0x60030271;
    RCC->AHB3RSTR = 0x00E95011;
    RCC->AHB4RSTR = 0x132806FF;

    RCC->APB1LRSTR = 0xEAFFC3FF;
    RCC->APB1LRSTR = 0x03000136;
    RCC->APB2RSTR = 0x40A730F3;
    RCC->APB3RSTR = 0x00000008;
    RCC->APB4RSTR = 0x0420DEAA;

    RCC->AHB1RSTR = 0;
    RCC->AHB2RSTR = 0;
    RCC->AHB3RSTR = 0;
    RCC->AHB4RSTR = 0;

    RCC->APB1LRSTR = 0;
    RCC->APB1LRSTR = 0;
    RCC->APB2RSTR = 0;
    RCC->APB3RSTR = 0;
    RCC->APB4RSTR = 0;
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
