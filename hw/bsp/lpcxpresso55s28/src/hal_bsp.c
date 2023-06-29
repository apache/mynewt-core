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
#include <stdint.h>
#include <errno.h>
#include <sys/types.h>
#include <stdio.h>
#include <os/mynewt.h>
#include <bsp/bsp.h>
#include <hal/hal_bsp.h>
#include <hal/hal_flash_int.h>
#include <flash_map/flash_map.h>
#include <hal/hal_flash.h>
#include <fsl_iap.h>
#include <fsl_iap_ffr.h>
#if MYNEWT_VAL(TIMER_0)
#include <hal/hal_timer.h>
#endif
#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1) || MYNEWT_VAL(UART_2) || \
    MYNEWT_VAL(UART_3) || MYNEWT_VAL(UART_4) || MYNEWT_VAL(UART_5)
#include <uart/uart.h>
#include <uart_hal/uart_hal.h>
#include <hal/hal_uart.h>
#endif
#include <mcu/cmsis_nvic.h>
#include <fsl_device_registers.h>
#include <fsl_common.h>
#include <fsl_clock.h>
#include <clock_config.h>
#include <mcu/mcux_hal.h>

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
#endif
#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
#endif
#if MYNEWT_VAL(UART_2)
static struct uart_dev os_bsp_uart2;
#endif
#if MYNEWT_VAL(UART_3)
static struct uart_dev os_bsp_uart3;
#endif
#if MYNEWT_VAL(UART_4)
static struct uart_dev os_bsp_uart4;
#endif
#if MYNEWT_VAL(UART_5)
static struct uart_dev os_bsp_uart5;
#endif

/*
 * What memory to include in coredump.
 */
static const struct hal_bsp_mem_dump dump_cfg[] = {
    [0] = {
        .hbmd_start = &__DATA_ROM,
        .hbmd_size = RAM_SIZE
    }
};

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    if (id == 0) {
        return &mcux_flash_dev;
    }
    return NULL;
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
    return 0;
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

static flash_config_t config;

void
hal_bsp_init(void)
{
    int rc = 0;

    (void)rc;

    FFR_Init(&config);

#if MYNEWT_VAL(TIMER_0)
    rc = hal_timer_init(0, NULL);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_1)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart1, "uart1",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_2)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart2, "uart2",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_3)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart3, "uart3",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_4)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart4, "uart4",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
#if MYNEWT_VAL(UART_5)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart5, "uart5",
                       OS_DEV_INIT_PRIMARY, 0, uart_hal_init, NULL);
    assert(rc == 0);
#endif
}

int
hal_bsp_hw_id_len(void)
{
    return 16;
}

int
hal_bsp_hw_id(uint8_t *id, int max_len)
{
    if (max_len < 16) {
        return -1;
    }
    status_t status = FFR_GetUUID(&config, id);
    return status == kStatus_Success ? 16 : -1;
}



void
hal_bsp_deinit(void)
{
}
