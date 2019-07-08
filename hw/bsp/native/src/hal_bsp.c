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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include "os/mynewt.h"
#include <bsp/bsp.h>
#include "hal/hal_flash_int.h"
#include "flash_map/flash_map.h"
#include "uart/uart.h"
#include "uart_hal/uart_hal.h"
#include "mcu/native_bsp.h"
#include "mcu/mcu_hal.h"
#include "hal/hal_i2c.h"
#include "defs/sections.h"
#include "ef_tinycrypt/ef_tinycrypt.h"
#include <trng_sw/trng_sw.h>

#if MYNEWT_VAL(SIM_ACCEL_PRESENT)
#include "sim/sim_accel.h"
static struct sim_accel os_bsp_accel0;
#endif

static struct uart_dev os_bsp_uart0;
static struct uart_dev os_bsp_uart1;
static struct trng_sw_dev os_bsp_trng;
static pid_t mypid;
static struct trng_sw_dev_cfg os_bsp_trng_cfg = {
    .tsdc_entr = &mypid,
    .tsdc_len = sizeof(mypid)
};
static sec_data_secret struct eflash_tinycrypt_dev ef_dev0 = {
    .etd_dev = {
        .efd_hal = {
            .hf_itf = &enc_flash_funcs,
        },
        .efd_hwdev = &native_flash_dev
    }
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    switch (id) {
    case 0:
        return &native_flash_dev;
    case 1:
        return &ef_dev0.etd_dev.efd_hal;
    default:
        return NULL;
    }
}

int
hal_bsp_power_state(int state)
{
    return (0);
}

#if MYNEWT_VAL(SIM_ACCEL_PRESENT)
int
simaccel_init(struct os_dev *odev, void *arg)
{
    return 0;
}
#endif

void
hal_bsp_init(void)
{
    int rc;

    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
            OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *) NULL);
    assert(rc == 0);

    rc = os_dev_create((struct os_dev *) &os_bsp_uart1, "uart1",
            OS_DEV_INIT_PRIMARY, 0, uart_hal_init, (void *) NULL);
    assert(rc == 0);

    mypid = getpid();
    rc = os_dev_create((struct os_dev *)&os_bsp_trng, "trng",
                       OS_DEV_INIT_PRIMARY, 0, trng_sw_dev_init,
                       &os_bsp_trng_cfg);
    assert(rc == 0);

#if MYNEWT_VAL(I2C_0)
    rc = hal_i2c_init(0, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(SIM_ACCEL_PRESENT)
    rc = os_dev_create((struct os_dev *) &os_bsp_accel0, "simaccel0",
            OS_DEV_INIT_PRIMARY, 0, simaccel_init, (void *) NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(OS_CPUTIME_TIMER_NUM >= 0)
    rc = os_cputime_init(MYNEWT_VAL(OS_CPUTIME_FREQ));
    assert(rc == 0);
#endif
}

void
hal_bsp_init_trng(void)
{
    int i;
    int rc;

    /*
     * Add entropy (don't do it like this if you use this for real, use
     * something proper).
     */
    for (i = 0; i < 8; i++) {
        rc = trng_sw_dev_add_entropy(&os_bsp_trng, &mypid, sizeof(mypid));
        assert(rc == 0);
    }
}
