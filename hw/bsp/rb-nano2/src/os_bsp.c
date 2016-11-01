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
#include <stdint.h>
#include "os/os_cputime.h"
#include "syscfg/syscfg.h"
#include "sysflash/sysflash.h"
#include "flash_map/flash_map.h"
#include "hal/hal_bsp.h"
#include "hal/hal_flash.h"
#include "hal/hal_spi.h"
#include "hal/hal_watchdog.h"
#include "mcu/nrf52_hal.h"
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#include "os/os_dev.h"
#include "bsp.h"

#if MYNEWT_VAL(SPI_MASTER)
#include "nrf_drv_spi.h"
#endif
#if MYNEWT_VAL(SPI_SLAVE)
#include "nrf_drv_spis.h"
#endif
#include "nrf_drv_config.h"
#include "app_util_platform.h"
#include "nrf.h"
#include "app_error.h"
#include "nrf_drv_saadc.h"

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct nrf52_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
};
#endif

void
hal_bsp_init(void)
{
    int rc;

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1)
    rc = hal_timer_init(1, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_2)
    rc = hal_timer_init(2, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_3)
    rc = hal_timer_init(3, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_4)
    rc = hal_timer_init(4, NULL);
    assert(rc == 0);
#endif

    /* Set cputime to count at 1 usec increments */
    rc = os_cputime_init(MYNEWT_VAL(CLOCK_FREQ));
    assert(rc == 0);

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&os_bsp_uart0_cfg);
    assert(rc == 0);
#endif
}
