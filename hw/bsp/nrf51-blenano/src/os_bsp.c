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
#include "hal/flash_map.h"
#if MYNEWT_VAL(UART_0)
#include "mcu/nrf51_hal.h"
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#endif
#include "os/os_dev.h"
#include "app_util_platform.h"

static struct flash_area bsp_flash_areas[] = {
    [FLASH_AREA_BOOTLOADER] = {
        .fa_flash_id = 0,       /* internal flash */
        .fa_off = 0x00000000,   /* beginning */
        .fa_size = (32 * 1024)
    },
    [FLASH_AREA_IMAGE_0] = {
        .fa_flash_id = 0,
        .fa_off = 0x00008000,
        .fa_size = (110 * 1024)
    },
    [FLASH_AREA_IMAGE_1] = {
        .fa_flash_id = 0,
        .fa_off = 0x00023800,
        .fa_size = (110 * 1024)
    },
    [FLASH_AREA_IMAGE_SCRATCH] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003f000,
        .fa_size = (2 * 1024)
    },
    [FLASH_AREA_NFFS] = {
        .fa_flash_id = 0,
        .fa_off = 0x0003f800,
        .fa_size = (2 * 1024)
    }
};

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct nrf51_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = MYNEWT_VAL(UART_0_PIN_TX),
    .suc_pin_rx = MYNEWT_VAL(UART_0_PIN_RX),
    .suc_pin_rts = MYNEWT_VAL(UART_0_PIN_RTS),
    .suc_pin_cts = MYNEWT_VAL(UART_0_PIN_CTS),
};
#endif

void *_sbrk(int incr);
void _close(int fd);

/*
 * Returns the flash map slot where the currently active image is located.
 * If executing from internal flash from fixed location, that slot would
 * be easy to find.
 * If images are in external flash, and copied to RAM for execution, then
 * this routine would have to figure out which one of those slots is being
 * used.
 */
int
bsp_imgr_current_slot(void)
{
    return FLASH_AREA_IMAGE_0;
}

void
os_bsp_init(void)
{
    int rc;

    (void)rc;

    /*
     * XXX this reference is here to keep this function in.
     */
    _sbrk(0);
    _close(0);

    flash_area_init(bsp_flash_areas,
      sizeof(bsp_flash_areas) / sizeof(bsp_flash_areas[0]));

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&os_bsp_uart0_cfg);
    assert(rc == 0);
#endif
}
