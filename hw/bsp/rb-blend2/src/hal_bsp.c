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
#include "os/mynewt.h"
#include "nrfx.h"
#include "bsp/bsp.h"
#include "hal/hal_bsp.h"
#include "hal/hal_system.h"
#include "mcu/nrf52_hal.h"
#include "mcu/nrf52_periph.h"
#include "flash_map/flash_map.h"
#include "hal/hal_flash.h"
#include "hal/hal_spi.h"
#include "hal/hal_i2c.h"
#include "hal/hal_watchdog.h"
#if MYNEWT_VAL(SOFT_PWM)
#include "pwm/pwm.h"
#include "soft_pwm/soft_pwm.h"
#endif
#if MYNEWT_VAL(UARTBB_0)
#include "uart_bitbang/uart_bitbang.h"
#endif

#if MYNEWT_VAL(SOFT_PWM)
static struct pwm_dev os_bsp_spwm[MYNEWT_VAL(SOFT_PWM_DEVS)];
#endif

#if MYNEWT_VAL(UARTBB_0)
static const struct uart_bitbang_conf os_bsp_uartbb0_cfg = {
    .ubc_txpin = MYNEWT_VAL(UARTBB_0_PIN_TX),
    .ubc_rxpin = MYNEWT_VAL(UARTBB_0_PIN_RX),
    .ubc_cputimer_freq = MYNEWT_VAL(OS_CPUTIME_FREQ),
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
    return &nrf52k_flash_dev;
}

const struct hal_bsp_mem_dump *
hal_bsp_core_dump(int *area_cnt)
{
    *area_cnt = sizeof(dump_cfg) / sizeof(dump_cfg[0]);
    return dump_cfg;
}

int
hal_bsp_power_state(int state)
{
    return (0);
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
    uint32_t cfg_pri;

    switch (irq_num) {
    /* Radio gets highest priority */
    case RADIO_IRQn:
        cfg_pri = 0;
        break;
    default:
        cfg_pri = pri;
    }
    return cfg_pri;
}

void
hal_bsp_init(void)
{
#if MYNEWT_VAL(SOFT_PWM)
    int rc;
    int idx;
    char *spwm_name;
#endif

    /* Make sure system clocks have started */
    hal_system_clock_start();

    /* Create all available nRF52840 peripherals */
    nrf52_periph_create();

#if MYNEWT_VAL(SOFT_PWM)
    for (idx = 0; idx < MYNEWT_VAL(SOFT_PWM_DEVS); idx++) {
        asprintf(&spwm_name, "spwm%d", idx);
        rc = os_dev_create(&os_bsp_spwm[idx].pwm_os_dev, spwm_name,
                           OS_DEV_INIT_KERNEL, OS_DEV_INIT_PRIO_DEFAULT,
                           soft_pwm_dev_init, UINT_TO_POINTER(idx));
        assert(rc == 0);
    }
#endif

#if MYNEWT_VAL(UARTBB_0)
    rc = os_dev_create(&os_bsp_uartbb0.ud_dev, "uartbb0",
                       OS_DEV_INIT_PRIMARY, 0, uart_bitbang_init,
                       (void *)&os_bsp_uartbb0_cfg);
    assert(rc == 0);
#endif
}
