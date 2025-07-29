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
#include <stddef.h>
#include <assert.h>
#include <inttypes.h>
#include <os/os_cputime.h>
#include "syscfg/syscfg.h"
#include "hal/hal_bsp.h"
#include "bsp/bsp.h"
#include "sysinit/sysinit.h"
#include "mcu/samd21.h"
#include "bsp/bsp.h"
#include "mynewt_cm.h"

/*
 * hw/mcu/atmel/samd21xx/src/sam0/drivers/tc/tc.h
 */
#include <tc.h>

#include "mcu/samd21_hal.h"
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "hal/hal_flash.h"
#include "mcu/hal_spi.h"
#include "mcu/hal_i2c.h"

/*
 * hw/mcu/atmel/samd21xx/src/sam0/drivers/sercom/usart/usart.h
 */
#include <usart.h>

#include <os/os_dev.h>
#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#include <mcu/hal_uart.h>

#endif
#if MYNEWT_VAL(SPI_0)
/* configure the SPI port for arduino external spi */
struct samd21_spi_config icsp_spi_config = {
    .dipo = 0,  /* sends MISO to PAD 0 */
    .dopo = 1,  /* send CLK to PAD 3 and MOSI to PAD 2 */
    .pad0_pinmux = PINMUX_PA12D_SERCOM4_PAD0,       /* MISO */
    .pad3_pinmux = PINMUX_PB11D_SERCOM4_PAD3,       /* SCK */
    .pad2_pinmux = PINMUX_PB10D_SERCOM4_PAD2,       /* MOSI */
};
#endif

#if MYNEWT_VAL(SPI_1)
/* NOTE using this will overwrite the debug interface */
struct samd21_spi_config alt_spi_config = {
    .dipo = 3,  /* sends MISO to PAD 3 */
    .dopo = 0,  /* send CLK to PAD 1 and MOSI to PAD 0 */
    .pad0_pinmux = PINMUX_PA04D_SERCOM0_PAD0,       /* MOSI */
    .pad1_pinmux = PINMUX_PA05D_SERCOM0_PAD1,       /* SCK */
    .pad2_pinmux = PINMUX_PA06D_SERCOM0_PAD2,       /* SCK */
    .pad3_pinmux = PINMUX_PA07D_SERCOM0_PAD3,       /* MISO */
};
#endif

#if MYNEWT_VAL(I2C_5)
struct samd21_i2c_config i2c_config = {
    .pad0_pinmux = PINMUX_PA22D_SERCOM5_PAD0,
    .pad1_pinmux = PINMUX_PA23D_SERCOM5_PAD1,
};
#endif

#if MYNEWT_VAL(UART_0)
static const struct samd21_uart_config uart0_cfg = {
    .suc_sercom = SERCOM5,
    .suc_mux_setting = USART_RX_3_TX_2_XCK_3,
    .suc_generator_source = GCLK_GENERATOR_0,
    .suc_sample_rate = USART_SAMPLE_RATE_16X_ARITHMETIC,
    .suc_sample_adjustment = USART_SAMPLE_ADJUSTMENT_7_8_9,
    .suc_pad0 = PINMUX_UNUSED,
    .suc_pad1 = PINMUX_UNUSED,
    .suc_pad2 = PINMUX_PB22D_SERCOM5_PAD2,
    .suc_pad3 = PINMUX_PB23D_SERCOM5_PAD3,
};
#endif

#if MYNEWT_VAL(UART_1)
static const struct samd21_uart_config uart1_cfg = {
    .suc_sercom = SERCOM2,
    .suc_mux_setting = USART_RX_3_TX_2_XCK_3,
    .suc_generator_source = GCLK_GENERATOR_0,
    .suc_sample_rate = USART_SAMPLE_RATE_16X_ARITHMETIC,
    .suc_sample_adjustment = USART_SAMPLE_ADJUSTMENT_7_8_9,
    .suc_pad0 = PINMUX_UNUSED,
    .suc_pad1 = PINMUX_UNUSED,
    .suc_pad2 = PINMUX_PA10D_SERCOM2_PAD2,
    .suc_pad3 = PINMUX_PA11D_SERCOM2_PAD3

};
#endif

/*
 * What memory to include in coredump.
 */
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &_ram_start,
        .hbmd_size = RAM_SIZE
    }
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }

    return &samd21_flash_dev;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
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
    int rc;

    (void)rc;
#if MYNEWT_VAL(TIMER_0) || MYNEWT_VAL(TIMER_1) || MYNEWT_VAL(TIMER_2)
    struct samd21_timer_cfg tmr_cfg;
#endif

#if MYNEWT_VAL(UART_0)
    static struct uart_dev hal_uart0;
    rc = os_dev_create((struct os_dev *)&hal_uart0, "uart0", OS_DEV_INIT_PRIMARY,
                       0, uart_hal_init, (void *)&uart0_cfg);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

#if MYNEWT_VAL(UART_1)
    static struct uart_dev hal_uart1;
    rc = os_dev_create((struct os_dev *)&hal_uart1, "uart1", OS_DEV_INIT_PRIMARY,
                       0, uart_hal_init, (void *)&uart1_cfg);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

#if MYNEWT_VAL(TIMER_0)
    tmr_cfg.clkgen = GCLK_GENERATOR_2;
    tmr_cfg.src_clock = GCLK_SOURCE_OSC8M;
    tmr_cfg.hwtimer = TC3;
    tmr_cfg.irq_num = TC3_IRQn;
    rc = hal_timer_init(0, &tmr_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1)
    tmr_cfg.clkgen = GCLK_GENERATOR_5;
    tmr_cfg.src_clock = GCLK_SOURCE_OSC8M;
    tmr_cfg.hwtimer = TC4;
    tmr_cfg.irq_num = TC4_IRQn;
    rc = hal_timer_init(1, &tmr_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_2)
    tmr_cfg.clkgen = GCLK_GENERATOR_6;
    tmr_cfg.src_clock = GCLK_SOURCE_OSC8M;
    tmr_cfg.hwtimer = TC5;
    tmr_cfg.irq_num = TC5_IRQn;
    rc = hal_timer_init(2, &tmr_cfg);
    assert(rc == 0);
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    /*
     * Set cputime to count at 1 usec increments.
     */
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0)
    rc = hal_spi_init(0, &icsp_spi_config, MYNEWT_VAL(SPI_0_TYPE));
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

#if MYNEWT_VAL(SPI_1)
    rc = hal_spi_init(1, &alt_spi_config, MYNEWT_VAL(SPI_1_TYPE));
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif

#if MYNEWT_VAL(I2C_5)
    rc = hal_i2c_init(5, &i2c_config);
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}

void
hal_bsp_deinit(void)
{
    Cortex_DisableAll();
}
