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


/**
 * @addtogroup HAL
 * @{
 *   @defgroup HALI2c HAL I2c
 *   @{
 */

#ifndef H_HAL_I2C_
#define H_HAL_I2C_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This is the API for an i2c bus.  Currently, this is a master API
 * allowing the mynewt device to function as an I2C master.
 *
 * A slave API is pending for future release
 *
 * Typical usage of this API is as follows:
 *
 * Initialize an i2c device with:
 *      :c:func:`hal_i2c_init()`
 *
 * When you wish to perform an i2c transaction, you call one or both of:
 *      :c:func:`hal_i2c_master_write()`;
 *      :c:func:`hal_i2c_master_read()`;
 *
 * These functions will issue a START condition, followed by the device's
 * 7-bit I2C address, and then send or receive the payload based on the data
 * provided. This will cause a repeated start on the bus, which is valid in
 * I2C specification, and the decision to use repeated starts was made to
 * simplify the I2C HAL. To set the STOP condition at an appropriate moment,
 * you set the `last_op` field to a `1` in either function.
 *
 * For example, in an I2C memory access you might write a register address and
 * then read data back via:
 *      :c:func:`hal_i2c_write()`; -- write to a specific register on the device
 *      :c:func:`hal_i2c_read()`; --- read back data, setting 'last_op' to '1'
 */

/**
 * When sending a packet, use this structure to pass the arguments.
 */
struct hal_i2c_master_data {
    /**
     * Destination address
     * An I2C address has 7 bits. In the protocol these
     * 7 bits are combined with a 1 bit R/W bit to specify read
     * or write operation in an 8-bit address field sent to
     * the remote device.  This API accepts the 7-bit
     * address as its argument in the 7 LSBs of the
     * address field above.  For example if I2C was
     * writing a 0x81 in its protocol, you would pass
     * only the top 7-bits to this function as 0x40
     */
    uint8_t  address
    /** Number of buffer bytes to transmit or receive */;
    uint16_t len;
    /** Buffer space to hold the transmit or receive */
    uint8_t *buffer;
};

/**
 * Initialize a new i2c device with the I2C number.
 *
 * @param i2c_num The number of the I2C device being initialized
 * @param cfg The hardware specific configuration structure to configure
 *            the I2C with.  This includes things like pin configuration.
 *
 * @return 0 on success, and non-zero error code on failure
 */
int hal_i2c_init(uint8_t i2c_num, void *cfg);

/**
 * Sends a start condition and writes <len> bytes of data on the i2c bus.
 * This API does NOT issue a stop condition unless `last_op` is set to `1`.
 * You must stop the bus after successful or unsuccessful write attempts.
 * This API is blocking until an error or NaK occurs. Timeout is platform
 * dependent.
 *
 * @param i2c_num The number of the I2C device being written to
 * @param pdata The data to write to the I2C bus
 * @param timeout How long to wait for transaction to complete in ticks
 * @param last_op Master should send a STOP at the end to signify end of
 *        transaction.
 *
 * @return 0 on success, and non-zero error code on failure
 */
int hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                         uint32_t timeout, uint8_t last_op);

/**
 * Sends a start condition and reads <len> bytes of data on the i2c bus.
 * This API does NOT issue a stop condition unless `last_op` is set to `1`.
 * You must stop the bus after successful or unsuccessful write attempts.
 * This API is blocking until an error or NaK occurs. Timeout is platform
 * dependent.
 *
 * @param i2c_num The number of the I2C device being written to
 * @param pdata The location to place read data
 * @param timeout How long to wait for transaction to complete in ticks
 * @param last_op Master should send a STOP at the end to signify end of
 *        transaction.
 *
 * @return 0 on success, and non-zero error code on failure
 */
int hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                         uint32_t timeout, uint8_t last_op);

/**
 * Probes the i2c bus for a device with this address.  THIS API
 * issues a start condition, probes the address using a read
 * command and issues a stop condition.
 *
 * @param i2c_num The number of the I2C to probe
 * @param address The address to probe for
 * @param timeout How long to wait for transaction to complete in ticks
 *
 * @return 0 on success, non-zero error code on failure
 */
int hal_i2c_master_probe(uint8_t i2c_num, uint8_t address,
                         uint32_t timeout);

#ifdef __cplusplus
}
#endif

#endif /* H_HAL_I2C_ */


/**
 *   @} HALI2c
 * @} HAL
 */
