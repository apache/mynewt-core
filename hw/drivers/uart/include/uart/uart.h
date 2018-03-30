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

#ifndef __DRIVERS_UART_H_
#define __DRIVERS_UART_H_

#include <inttypes.h>
#include "os/mynewt.h"

#ifdef __cplusplus
extern "C" {
#endif

struct uart_dev;

/*
 * Function prototype for UART driver to ask for more data to send.
 * Driver must call this with interrupts disabled.
 *
 * @param arg		This is uc_cb_arg passed in uart_conf in
 *			os_dev_open().
 *
 * @return		Byte value to send next, -1 if no more data to send.
 */
typedef int (*uart_tx_char)(void *arg);

/*
 * Function prototype for UART driver to report that transmission is
 * complete. This should be called when transmission of last byte is
 * finished.
 * Driver must call this with interrupts disabled.
 *
 * @param arg		This is uc_cb_arg passed in uart_conf in
 *			os_dev_open().
 */
typedef void (*uart_tx_done)(void *arg);

/*
 * Function prototype for UART driver to report incoming byte of data.
 * Driver must call this with interrupts disabled.
 *
 * @param arg		This is uc_cb_arg passed in uart_conf in
 *			os_dev_open().
 * @param byte		Received byte
 *
 * @return		0 on success; -1 if data could not be received.
 *			Note that returning -1 triggers flow control (if
 *			configured), and user must call uart_start_rx() to
 *			start receiving more data again.
 */
typedef int (*uart_rx_char)(void *arg, uint8_t byte);

struct uart_driver_funcs {
    void (*uf_start_tx)(struct uart_dev *);
    void (*uf_start_rx)(struct uart_dev *);
    void (*uf_blocking_tx)(struct uart_dev *, uint8_t);
};

struct uart_dev {
    struct os_dev ud_dev;
    struct uart_driver_funcs ud_funcs;
    void *ud_priv;
};

/*
 * ***Note that these enum values must match what hw/hal/hal_uart.h***
 */
enum uart_parity {
    UART_PARITY_NONE = 0,       /* no parity */
    UART_PARITY_ODD = 1,        /* odd parity bit */
    UART_PARITY_EVEN = 2        /* even parity bit */
};

enum uart_flow_ctl {
    UART_FLOW_CTL_NONE = 0,     /* no flow control */
    UART_FLOW_CTL_RTS_CTS = 1   /* RTS/CTS */
};

struct uart_conf {
    uint32_t uc_speed;          /* baudrate in bps */
    uint8_t uc_databits;        /* number of data bits */
    uint8_t uc_stopbits;        /* number of stop bits */
    enum uart_parity uc_parity;
    enum uart_flow_ctl uc_flow_ctl;
    uart_tx_char uc_tx_char;
    uart_rx_char uc_rx_char;
    uart_tx_done uc_tx_done;
    void *uc_cb_arg;
};

/*
 * Tell driver that more data has been queued, and that the driver should
 * start asking for it.
 *
 * @param dev		Uart device in question
 */
static inline void
uart_start_tx(struct uart_dev *dev)
{
    dev->ud_funcs.uf_start_tx(dev);
}

/*
 * Tell driver that previous report on RX buffer shortage is now done,
 * and that app is ready to receive more data.
 *
 * @param dev		Uart device in question
 */
static inline void
uart_start_rx(struct uart_dev *dev)
{
    dev->ud_funcs.uf_start_rx(dev);
}

/*
 * Blocking transmit. ***Note that this should only be used with console.
 * And only when MCU is about to restart, to write final log messages***
 *
 * @param dev		Uart device in question
 */
static inline void
uart_blocking_tx(struct uart_dev *dev, uint8_t byte)
{
    dev->ud_funcs.uf_blocking_tx(dev, byte);
}

#ifdef __cplusplus
}
#endif

#endif /* __DRIVERS_UART_H_ */
