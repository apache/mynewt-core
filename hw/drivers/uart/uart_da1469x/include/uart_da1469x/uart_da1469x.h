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

#ifndef __DRIVERS_UART_DA1469X_H_
#define __DRIVERS_UART_DA1469X_H_

#include "uart/uart.h"
#include "mcu/da1469x_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

struct da1469x_uart_hw_data;

struct da1469x_uart_dev {
    struct uart_dev dev;
    /* Common UART parameters. */
    struct uart_conf uc;
    /* DA1469x specific configuration. */
    struct da1469x_uart_cfg da1469x_cfg;

    /* driver state data */
    uint8_t active : 1;
    uint8_t tx_started : 1;
    uint8_t rx_started : 1;
    uint8_t rx_stalled : 1;
    uint8_t rx_data;

    /* Callout used to re-enable UART after it RX pin went high. */
    struct os_callout wakeup_callout;
    /* Event raised from interrupt (busy/break) that will setup RX pin to GPIO with interrupt in task context. */
    struct os_event setup_wakeup_event;
    /* Hardware configuration, register addresses bit mask and such. */
    const struct da1469x_uart_hw_data *hw;
};

/**
 * Creates UART OS device
 *
 * @param dev - device structure to initialize
 * @param name - device name (must end with character 0|1|2 like "uart0")
 * @param priority - priority for os_dev_create
 * @param da1469x_cfg - UART parameters
 * @return 0 on success,
 */
int da1469x_uart_dev_create(struct da1469x_uart_dev *dev, const char *name, uint8_t priority,
                            const struct da1469x_uart_cfg *da1469x_cfg);

#ifdef __cplusplus
}
#endif

#endif /* __DRIVERS_UART_DA1469X_H_ */
