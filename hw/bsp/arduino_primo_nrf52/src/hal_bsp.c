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
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#include "bsp/bsp.h"
#include <hal/hal_bsp.h>
#include "mcu/nrf52_hal.h"

#include <hal/hal_cputime.h>
#include <os/os_dev.h>
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#include <uart_bitbang/uart_bitbang.h>

static const struct nrf52_uart_cfg uart0_cfg = {
    .suc_pin_tx = 6,
    .suc_pin_rx = 5,
    .suc_pin_rts = 0,
    .suc_pin_cts = 0
};

static const struct uart_bitbang_conf uart1_cfg = {
    .ubc_rxpin = 11,
    .ubc_txpin = 12,
    .ubc_cputimer_freq = 1000000,
};

static struct uart_dev hal_uart0;
static struct uart_dev bitbang_uart1;

/*
 * What memory to include in coredump.
 */
static const struct bsp_mem_dump dump_cfg[] = {
    [0] = {
	.bmd_start = &_ram_start,
        .bmd_size = RAM_SIZE
    }
};

const struct hal_flash *
bsp_flash_dev(uint8_t id)
{
    /*
     * Internal flash mapped to id 0.
     */
    if (id != 0) {
        return NULL;
    }
    return &nrf52k_flash_dev;
}

const struct bsp_mem_dump *
bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

int
bsp_hal_init(void)
{
    int rc;

    rc = os_dev_create((struct os_dev *) &hal_uart0, "uart0",
      OS_DEV_INIT_PRIMARY, OS_DEV_INIT_PRIO_DEFAULT,
      uart_hal_init, (void *)&uart0_cfg);
    assert(rc == 0);

    /*
     * Need to initialize cputime here, because bitbanger uart uses it.
     */
    rc = cputime_init(1000000);
    assert(rc == 0);

    rc = os_dev_create((struct os_dev *) &bitbang_uart1, "uart1",
      OS_DEV_INIT_PRIMARY, 0,
      uart_bitbang_init, (void *)&uart1_cfg);
    assert(rc == 0);
    return 0;
}
