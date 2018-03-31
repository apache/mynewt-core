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

#include <ctype.h>
#include <assert.h>
#include <string.h>

#include "os/mynewt.h"
#include <hal/hal_uart.h>

#include <uart/uart.h>

#include "uart_hal/uart_hal.h"

inline static int
uart_hal_dev_get_id(struct uart_dev *dev)
{
    return (intptr_t)(dev->ud_priv) - 1;
}

inline static void
uart_hal_dev_set_id(struct uart_dev *dev, int id)
{
    dev->ud_priv = (void *)((intptr_t)(id + 1));
}

static void
uart_hal_start_tx(struct uart_dev *dev)
{
    assert(dev->ud_priv);

    hal_uart_start_tx(uart_hal_dev_get_id(dev));
}

static void
uart_hal_start_rx(struct uart_dev *dev)
{
    assert(dev->ud_priv);

    hal_uart_start_rx(uart_hal_dev_get_id(dev));
}

static void
uart_hal_blocking_tx(struct uart_dev *dev, uint8_t byte)
{
    assert(dev->ud_priv);

    hal_uart_blocking_tx(uart_hal_dev_get_id(dev), byte);
}

static int
uart_hal_open(struct os_dev *odev, uint32_t wait, void *arg)
{
    struct uart_conf *uc;
    struct uart_dev *dev;
    int rc;

    dev = (struct uart_dev *)odev;
    uc = (struct uart_conf *)arg;

    assert(dev->ud_priv);

    if (!uc) {
        return OS_EINVAL;
    }
    if (odev->od_flags & OS_DEV_F_STATUS_OPEN) {
        return OS_EBUSY;
    }
    rc = hal_uart_init_cbs(uart_hal_dev_get_id(dev), uc->uc_tx_char, uc->uc_tx_done,
                           uc->uc_rx_char, uc->uc_cb_arg);
    if (rc) {
        return OS_EINVAL;
    }

    rc = hal_uart_config(uart_hal_dev_get_id(dev), uc->uc_speed, uc->uc_databits,
      uc->uc_stopbits, (enum hal_uart_parity)uc->uc_parity, (enum hal_uart_flow_ctl)uc->uc_flow_ctl);
    if (rc) {
        return OS_EINVAL;
    }
    return OS_OK;
}

static int
uart_hal_close(struct os_dev *odev)
{
    struct uart_dev *dev;
    int rc;

    dev = (struct uart_dev *)odev;
    rc = hal_uart_close(uart_hal_dev_get_id(dev));
    if (rc) {
        return OS_EINVAL;
    }
    return OS_OK;
}

/*
 * Arg points to BSP specific UART configuration.
 */
int
uart_hal_init(struct os_dev *odev, void *arg)
{
    struct uart_dev *dev;
    char ch;

    dev = (struct uart_dev *)odev;

    ch = odev->od_name[strlen(odev->od_name) - 1];
    if (!isdigit((int) ch)) {
        return OS_EINVAL;
    }
    uart_hal_dev_set_id(dev, ch - '0');

    OS_DEV_SETHANDLERS(odev, uart_hal_open, uart_hal_close);

    dev->ud_funcs.uf_start_tx = uart_hal_start_tx;
    dev->ud_funcs.uf_start_rx = uart_hal_start_rx;
    dev->ud_funcs.uf_blocking_tx = uart_hal_blocking_tx;

    hal_uart_init(uart_hal_dev_get_id(dev), arg);

    return OS_OK;
}
