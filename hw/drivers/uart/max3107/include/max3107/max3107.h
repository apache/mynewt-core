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

#ifndef __DRIVERS_MAX3107_H_
#define __DRIVERS_MAX3107_H_

#include <stdint.h>
#include <uart/uart.h>
#include <hal/hal_spi.h>
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
#include <bus/drivers/spi_common.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct max3107_dev;

struct max3107_cfg {
#if MYNEWT_VAL(BUS_DRIVER_PRESENT)
    struct bus_spi_node_cfg node_cfg;
#else
    struct hal_spi_settings spi_settings;
    int spi_num;
    int ss_pin;
#endif
    /**< External clock/oscillator frequency in Hz */
    uint32_t osc_freq;
    /**< IRQ Pin */
    int8_t irq_pin;
    /** RX FIFO trigger level */
    uint8_t rx_trigger_level;
    /** TX FIFO trigger level */
    uint8_t tx_trigger_level;
    int8_t ldoen_pin;
    /**< 1 - External crystal oscillator, 0 - external clock */
    int8_t crystal_en: 1;
    /**< 1 - To disable PLL */
    uint8_t no_pll: 1;
    /**< 1 - To enable x 4 mode */
    uint8_t allow_mul_4: 1;
    /**< 1 - To enable x 4 mode */
    uint8_t allow_mul_2: 1;
};

typedef enum max3107_error {
    /** Error during SPI transaction device may be in unpredictable state, close device */
    MAX3107_ERROR_IO_FAILURE = 0x01,
    /** RX overrun, some data was lost */
    MAX3107_ERROR_UART_OVERRUN = 0x02,
    /** Parity error on incoming data */
    MAX3107_ERROR_UART_PARITY = 0x04,
    /** Framing error on incoming data */
    MAX3107_ERROR_UART_FRAMING = 0x08,
} max3107_error_t;

struct max3107_client {
    /**< Function that will be called from process context when data can be read. */
    void (*readable)(struct max3107_dev *dev);
    /**< Function that will be called from process context when data can be written. */
    void (*writable)(struct max3107_dev *dev);
    /**< Function that will be called from process context when break is detected. */
    void (*break_detected)(struct max3107_dev *dev);
    /**< Function that will be called from process context when receive errors are detected. */
    void (*error)(struct max3107_dev *dev, max3107_error_t errcode);
};

/**
 * Configure UART parameters.
 *
 * This function sets baud rate, data size, stop bits, parity, flow control.
 *
 * @param dev  - device to configure.
 * @param conf - UART parameters.
 *
 * @return 0 on success, error code otherwise.
 */
int max3107_config_uart(struct max3107_dev *dev, const struct uart_conf_port *conf);

/**
 * Write to UART TX FIFO.
 *
 * @param dev  - device to write to
 * @param buf  - data to write
 * @param size - number of bytes to write
 *
 * @return negative value on failure,
 *         0 output FIFO full or size is 0
 *         number of bytes written (can be less then size)
 */
int max3107_write(struct max3107_dev *dev, const void *buf, size_t size);

/**
 * Read UART RX FIFO bytes
 *
 * This function does not wait for data it just reads whatever is in RX FIFO.
 * Can return 0 if RX FIFO is empty.
 *
 * @param dev  - device to read from
 * @param buf  - data buffer
 * @param size - size of the data buffer
 * @return negative value on failure, 0-128 number of bytes read.
 */
int max3107_read(struct max3107_dev *dev, void *buf, size_t size);

/**
 * Get real baudrate of the UART
 *
 * Baudrate requested during opening of the device may not be possible with
 * clock that is supplied to max3107.
 * User can check what is the actual value baudrate calling this function.
 *
 * @param dev - device to check baudrate
 * @return baudrate of the device
 */
uint32_t max3107_get_real_baudrate(struct max3107_dev *dev);

/**
 * Returns number of bytes in RX FIFO that can be read.
 *
 * @param dev - MAX3107 device
 * @return 0-128 number of bytes that can be read, negative value on failure.
 */
int max3107_rx_available(struct max3107_dev *dev);

/**
 * Returns number of bytes that can be written to TX FIFO.
 *
 * @param dev - MAX3017 device
 * @return 0-128 number of bytes that can be written, negative value on failure.
 */
int max3107_tx_available(struct max3107_dev *dev);

/**
 * Set client data to already opened MAX3107 device.
 *
 * @param dev    - device to set client to
 * @param client - client structure (callbacks) can be NULL to stop receiving
 *                 notifications.
 * @return 0 on success, non-zero on failure
 */
int max3107_set_client(struct max3107_dev *dev, const struct max3107_client *client);

/**
 * Open MAX3107 device with specified client data
 *
 * @param name   - device name
 * @param client - client structure (callbacks)
 * @return pointer to device if device can be opened, otherwise NULL.
 */
struct max3107_dev *max3107_open(const char *name, const struct max3107_client *client);

/**
 * Returns max3107 device from opened UART device.
 *
 * This can be used to get underlying device.
 *
 * @param uart - uart opened with uart_open
 * @return underlying max3107_dev device.
 */
struct max3107_dev *max3107_get_dev_from_uart(struct uart_dev *uart);

/**
 * Create MAX3107 device.
 * This function will create pair of devices one that uses interface from this file and second
 * UART device that can be used like other UART devices.
 *
 * @param max3107_dev - device structure to initialized
 * @param name        - name of max3107 device
 * @param uart_name   - name of uart device (to be used like other UARTs)
 * @param cfg         - device configuration
 * @param uart_cfg    - uart configuration
 * @return 0 on success, non-zero on failure
 */
int max3107_dev_create_spi(struct max3107_dev *max3107_dev, const char *name, const char *uart_name,
                           const struct max3107_cfg *cfg, const struct uart_conf_port *uart_cfg);

#include "../../src/max3107_priv.h"

#ifdef __cplusplus
}
#endif

#endif /* __DRIVERS_MAX3107_H_ */
