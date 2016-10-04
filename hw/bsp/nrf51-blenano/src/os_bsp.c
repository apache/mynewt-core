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
#include "syscfg/syscfg.h"
#include "flash_map/flash_map.h"
#include "hal/hal_flash.h"
#include "hal/hal_bsp.h"
#include "hal/hal_spi.h"
#include "hal/hal_cputime.h"
#include "mcu/nrf51_hal.h"
#if MYNEWT_VAL(SPI_MASTER)
#include "nrf_drv_spi.h"
#endif
#if MYNEWT_VAL(SPI_SLAVE)
#include "nrf_drv_spis.h"
#endif
#include "nrf_drv_config.h"
#include "app_util_platform.h"
#include "os/os_dev.h"
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"


#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct nrf51_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
};
#endif

void
bsp_init(void)
{
    int rc;

    (void)rc;

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&os_bsp_uart0_cfg);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_MASTER)
    nrf_drv_spi_config_t spi_cfg = NRF_DRV_SPI_DEFAULT_CONFIG(0);
#endif
#if MYNEWT_VAL(SPI_SLAVE)
    nrf_drv_spis_config_t spi_cfg = NRF_DRV_SPIS_DEFAULT_CONFIG(1);
#endif

    /* Set cputime to count at 1 usec increments */
    rc = cputime_init(MYNEWT_VAL(CLOCK_FREQ));
    assert(rc == 0);

#if MYNEWT_VAL(SPI_MASTER)
    rc = hal_spi_init(0, &spi_cfg, HAL_SPI_TYPE_MASTER);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SPI_SLAVE)
    spi_cfg.csn_pin = SPI_SS_PIN;
    spi_cfg.csn_pullup = NRF_GPIO_PIN_PULLUP;
    rc = hal_spi_init(1, &spi_cfg, HAL_SPI_TYPE_SLAVE);
    assert(rc == 0);
#endif
}
