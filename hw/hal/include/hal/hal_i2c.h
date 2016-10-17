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
 * Initialize an i2c device with
 *      hal_i2c_init()
 *
 * When you with to perform an i2c transaction, issue
 *      hal_i2c_master_begin()l
 * followed by the transaction.  For example, in an I2C memory access access
 * you might write and address and then read back data
 *      hal_i2c_write(); -- write amemory ddress to device
 *      hal_i2c_read(); --- read back data
 * then end the transaction
 *      hal_i2c_end();
 */

/**
 * when sending a packet, use this structure to pass the arguments.
 */
struct hal_i2c_master_data {
    uint8_t  address;   /* destination address */
            /* a I2C address has 7 bits. In the protocol these
             * 7 bits are combined with a 1 bit R/W bit to specify read
             * or write operation in an 8-bit address field sent to
             * the remote device .  This API accepts the 7-bit
             * address as its argument in the 7 LSBs of the
             * address field above.  For example if I2C was
             * writing a 0x81 in its protocol, you would pass
             * only the top 7-bits to this function as 0x40 */
    uint16_t len;       /* number of buffer bytes to transmit or receive */
    uint8_t *buffer;    /* buffer space to hold the transmit or receive */
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
 * Sends a start condition and writes <len> bytes of data on the i2c.
 * This API assumes that you have already called hal_i2c_master_begin
 * It will fail if you have not. This API does NOT issue a stop condition.
 * You must stop the bus after successful or unsuccessful write attempts.
 * This API is blocking until an error or NaK occurs. Timeout is platform
 * dependent.
 *
 * @param i2c_num The number of the I2C device being written to
 * @param pdata The data to write to the I2C bus
 * @param timeout How long to wait for transaction to complete in ticks
 *
 * @return 0 on success, and non-zero error code on failure
 */
int hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                         uint32_t timeout);

/**
 * Sends a start condition and reads <len> bytes of data on the i2c.
 * This API assumes that you have already called hal_i2c_master_begin
 * It will fail if you have not. This API does NOT issue a stop condition.
 * You must stop the bus after successful or unsuccessful write attempts.
 * This API is blocking until an error or NaK occurs. Timeout is platform
 * dependent.
 *
 * @param i2c_num The number of the I2C device being written to
 * @param pdata The location to place read data
 * @param timeout How long to wait for transaction to complete in ticks
 *
 * @return 0 on success, and non-zero error code on failure
 */
int hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                        uint32_t timeout);

/**
 * Starts an I2C transaction with the driver. This API does not send
 * anything over the bus itself.
 *
 * @param i2c_num The number of the I2C to begin a transaction on
 *
 * @return 0 on success, non-zero error code on failure
 */
int hal_i2c_master_begin(uint8_t i2c_num);

/**
 * Issues a stop condition on the bus and ends the I2C transaction.
 * You must call i2c_master_end for every hal_i2c_master_begin
 * API call that succeeds.
 *
 * @param i2c_num The number of the I2C to end a transaction on
 *
 * @return 0 on success, non-zero error code on failure
 */
int hal_i2c_master_end(uint8_t i2c_num);

/**
 * Probes the i2c bus for a device with this address.  THIS API
 * issues a start condition, probes the address using a read
 * command and issues a stop condition.   There is no need to call
 * hal_i2c_master_begin/end with this method.
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
