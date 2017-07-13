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

#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include <os/os_cputime.h>
#include <syscfg/syscfg.h>
#include <sysflash/sysflash.h>
#include <flash_map/flash_map.h>
#include <hal/hal_bsp.h>
#include <hal/hal_flash.h>
#include <hal/hal_spi.h>
#include <hal/hal_watchdog.h>
#include <mcu/fe310_hal.h>
#if MYNEWT_VAL(UART_0)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#endif
#include <os/os_dev.h>
#include <bsp/bsp.h>
#include <env/freedom-e300-hifive1/platform.h>

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
static const struct fe310_uart_cfg os_bsp_uart0_cfg = {
    .suc_pin_tx = HIFIVE_UART0_TX,
    .suc_pin_rx = HIFIVE_UART0_RX,
};
#endif

#if MYNEWT_VAL(TIMER_0)
extern struct fe310_hal_tmr fe310_pwm2;
#endif
#if MYNEWT_VAL(TIMER_1)
extern struct fe310_hal_tmr fe310_pwm1;
#endif
#if MYNEWT_VAL(TIMER_2)
extern struct fe310_hal_tmr fe310_pwm0;
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
    return &fe310_flash_dev;
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
#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, (void *)&fe310_pwm2);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_1)
    rc = hal_timer_init(1, (void *)&fe310_pwm1);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(TIMER_2)
    rc = hal_timer_init(2, (void *)&fe310_pwm0);
    assert(rc == 0);
#endif

#if (MYNEWT_VAL(OS_CPUTIME_TIMER_NUM) >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *)&os_bsp_uart0_cfg);
    assert(rc == 0);
#endif

}
