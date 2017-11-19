/**
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

#include <syscfg/syscfg.h>

#include <hal/hal_bsp.h>
#include <bsp/bsp.h>
#include <hal/hal_spi.h>
#include <mcu/hal_apollo2.h>

#if MYNEWT_VAL(UART_0)
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#endif

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct apollo2_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
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

/*
 * NOTE: Our HAL expects that the SS pin, if used, is treated as a gpio line
 * and is handled outside the SPI routines.
 */

#if MYNEWT_VAL(SPI_0_MASTER)
static const struct apollo2_spi_cfg hal_bsp_spi0m_cfg = {
    .sck_pin      = 5,
    .mosi_pin     = 7,
    .miso_pin     = 6,
};
#endif

#if MYNEWT_VAL(SPI_1_MASTER)
static const struct apollo2_spi_cfg hal_bsp_spi1m_cfg = {
    .sck_pin      = 8,
    .mosi_pin     = 10,
    .miso_pin     = 9,
};
#endif

#if MYNEWT_VAL(SPI_2_MASTER)
static const struct apollo2_spi_cfg hal_bsp_spi2m_cfg = {
    .sck_pin      = 0,
    .mosi_pin     = 1,
    .miso_pin     = 2,
};
#endif

#if MYNEWT_VAL(SPI_3_MASTER)
static const struct apollo2_spi_cfg hal_bsp_spi3m_cfg = {
    .sck_pin      = 42,
    .mosi_pin     = 38,
    .miso_pin     = 43,
};
#endif

#if MYNEWT_VAL(SPI_4_MASTER)
static const struct apollo2_spi_cfg hal_bsp_spi4m_cfg = {
    .sck_pin      = 39,
    .mosi_pin     = 44,
    .miso_pin     = 40,
};
#endif

#if MYNEWT_VAL(SPI_5_MASTER)
static const struct apollo2_spi_cfg hal_bsp_spi5m_cfg = {
    .sck_pin      = 48,
    .mosi_pin     = 47,
    .miso_pin     = 49,
};
#endif

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    if (id != 0) {
        return (NULL);
    }
    return &apollo2_flash_dev;
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
    struct apollo2_timer_cfg timer_cfg;
    int rc;

    (void) timer_cfg;
    (void) rc;

#if MYNEWT_VAL(TIMER_0_SOURCE)
    timer_cfg.source = MYNEWT_VAL(TIMER_0_SOURCE);
    rc = hal_timer_init(0, &timer_cfg);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1_SOURCE)
    timer_cfg.source = MYNEWT_VAL(TIMER_1_SOURCE);
    rc = hal_timer_init(1, &timer_cfg);
    assert(rc == 0);
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
            OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *) &os_bsp_uart0_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_0_MASTER)
    rc = hal_spi_init(0, (void *)&hal_bsp_spi0m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_1_MASTER)
    rc = hal_spi_init(1, (void *)&hal_bsp_spi1m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_2_MASTER)
    rc = hal_spi_init(2, (void *)&hal_bsp_spi2m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_3_MASTER)
    rc = hal_spi_init(3, (void *)&hal_bsp_spi3m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_4_MASTER)
    rc = hal_spi_init(4, (void *)&hal_bsp_spi4m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_5_MASTER)
    rc = hal_spi_init(5, (void *)&hal_bsp_spi5m_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif
}

int
hal_bsp_hw_id(uint8_t *id, int max_len)
{
#if 0
    if (max_len > sizeof(DEVID)) {
        max_len = sizeof(DEVID);
    }

    memcpy(id, &DEVID, max_len);
    return max_len;
#endif
    return 0;
}
