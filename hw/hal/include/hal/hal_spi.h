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
#ifndef H_HAL_SPI_
#define H_HAL_SPI_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

/* SPI type */
#define HAL_SPI_TYPE_MASTER         (0)
#define HAL_SPI_TYPE_SLAVE          (1)

/* SPI modes */
#define HAL_SPI_MODE0               (0)
#define HAL_SPI_MODE1               (1)
#define HAL_SPI_MODE2               (2)
#define HAL_SPI_MODE3               (3)

/* SPI data order */
#define HAL_SPI_MSB_FIRST           (0)
#define HAL_SPI_LSB_FIRST           (1)

/* SPI word size */
#define HAL_SPI_WORD_SIZE_8BIT      (0)
#define HAL_SPI_WORD_SIZE_9BIT      (1)

/* Prototype for tx/rx callback */
typedef void (*hal_spi_txrx_cb)(void *arg, int len);

/**
 * since one spi device can control multiple devices, some configuration
 * can be changed on the fly from the hal
 */
struct hal_spi_settings {
    uint8_t         spi_type;
    uint8_t         data_mode;
    uint8_t         data_order;
    uint8_t         word_size;
    uint32_t        baudrate;
    hal_spi_txrx_cb txrx_cb_func;
    void            *txrx_cb_arg;
};

/**
 * Initialize the SPI, given by spi_num.
 *
 * @param spi_num The number of the SPI to initialize
 * @param cfg HW/MCU specific configuration,
 *            passed to the underlying implementation, providing extra
 *            configuration.
 * @param spi_type SPI type (master or slave)
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int hal_spi_init(int spi_num, void *cfg, uint8_t spi_type);

/**
 * Configure the spi.
 *
 * @param spi_num The number of the SPI to configure.
 * @param psettings The settings to configure this SPI with
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int hal_spi_config(int spi_num, struct hal_spi_settings *psettings);

/**
 * Sets the txrx callback (executed at interrupt context) when the
 * buffer is transferred by the master or the slave using the hal_spi_rxtx API.
 * Allowed to be called when the spi is enabled but not when a transfer is in
 * progress. This callback will also be called when chip select is de-asserted
 * on the slave.
 *
 * @param spi_num   SPI interface on which to set callback
 * @param txrx      Callback function
 * @param arg       Argument to be passed to callback function
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int hal_spi_set_txrx_cb(int spi_num, hal_spi_txrx_cb txrx_cb, void *arg);

/**
 * Enables the SPI. This does not start a transmit or receive operation;
 * it is used for power mgmt. Cannot be called when a SPI transfer is in
 * progress.
 *
 * @param spi_num
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int hal_spi_enable(int spi_num);

/**
 * Disables the SPI. Used for power mgmt. It will halt any current SPI transfers
 * in progress.
 *
 * @param spi_num
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int hal_spi_disable(int spi_num);

/**
 * Blocking call to send a value on the SPI. Returns the value received from the
 * SPI.
 *
 * MASTER: Sends the value and returns the received value from the slave.
 * SLAVE: Invalid API. Returns 0xFFFF
 *
 * @param spi_num   Spin interface to use
 * @param val       Value to send
 *
 * @return uint16_t Value received on SPI interface from slave. Returns 0xFFFF
 * if called when the SPI is configured to be a slave
 */
uint16_t hal_spi_tx_val(int spi_num, uint16_t val);

/**
 * Non-blocking call to send a buffer and also store the received values.
 * For both the master and slave, the txrx callback is executed at interrupt
 * context when the buffer is sent.
 *     MASTER: master sends all the values in the buffer and stores the
 *             stores the values in the receive buffer if rxbuf is not NULL.
 *             The txbuf parameter cannot be NULL
 *     SLAVE: Slave preloads the data to be sent to the master (values
 *            stored in txbuf) and places received data from master in rxbuf (if
 *            not NULL). No callback per received value; txrx callback when len
 *            values are transferred or master de-asserts chip select. If
 *            txbuf is NULL, the slave transfers its default byte. Both rxbuf
 *            and txbuf cannot be NULL.
 *
 * @param spi_num   SPI interface to use
 * @param txbuf     Pointer to buffer where values to transmit are stored.
 * @param rxbuf     Pointer to buffer to store values received from peer. Can
 *                  be NULL.
 * @param len       Number of values to be transferred.
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int hal_spi_txrx(int spi_num, void *txbuf, void *rxbuf, int len);

/**
 * Sets the default value transferred by the slave.
 *
 * @param spi_num SPI interface to use
 *
 * @return int 0 on success, non-zero error code on failure.
 */
int hal_spi_slave_set_def_tx_val(int spi_num, uint16_t val);

/**
 * This aborts the current transfer but keeps the spi enabled.
 *
 * @param spi_num   SPI interface on which transfer should be aborted.
 *
 * @return int 0 on success, non-zero error code on failure.
 *
 * NOTE: does not return an error if no transfer was in progress.
 */
int hal_spi_abort(int spi_num);

#ifdef __cplusplus
}
#endif

#endif /* H_HAL_SPI_ */
