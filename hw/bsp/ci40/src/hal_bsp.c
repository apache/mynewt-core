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
#include "hal/hal_bsp.h"
#include "bsp/bsp.h"
#include "syscfg/syscfg.h"
#include "uart/uart.h"
#if MYNEWT_VAL(UART_0) || MYNEWT_VAL(UART_1)
#include "uart_hal/uart_hal.h"
#endif

#include <assert.h>

const struct hal_flash *
hal_bsp_flash_dev(uint8_t id)
{
    return 0;
}

#if MYNEWT_VAL(UART_0)
static struct uart_dev os_bsp_uart0;
#endif

#if MYNEWT_VAL(UART_1)
static struct uart_dev os_bsp_uart1;
#endif

void _close(int fd);

void
hal_bsp_init(void)
{
    int rc;

#if MYNEWT_VAL(UART_0)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart0, "uart0",
        OS_DEV_INIT_PRIMARY, 0, uart_hal_init, 0);
    assert(rc == 0);
#endif

#if MYNEWT_VAL(UART_1)
    rc = os_dev_create((struct os_dev *) &os_bsp_uart1, "uart1",
        OS_DEV_INIT_PRIMARY, 0, uart_hal_init, 0);
    assert(rc == 0);
#endif

    (void)rc;
}
