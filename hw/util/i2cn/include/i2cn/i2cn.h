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

#ifndef H_I2CN_
#define H_I2CN_

#include <inttypes.h>
#include "os/mynewt.h"
struct hal_i2c_master_data;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Reads from an I2C slave, retrying the specified number of times on
 * failure.
 *
 * @param i2c_num               The index of the I2C interface to read from.
 * @param pdata                 Additional parameters describing the read
 *                                  operation.
 * @param timeout               The time, in OS ticks, to wait for operation
 *                                  completion.
 * @param last_op               1 if this is the final message in the
 *                                  transaction.
 *
 * @return                      0 on success;
 *                              HAL_I2C_ERR_[...] code on failure.
 */
int i2cn_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                     os_time_t timeout, uint8_t last_op, int retries);

/**
 * @brief Writes to an I2C slave, retrying the specified number of times on
 * failure.
 *
 * @param i2c_num               The index of the I2C interface to write to.
 * @param pdata                 Additional parameters describing the write
 *                                  operation.
 * @param timeout               The time, in OS ticks, to wait for operation
 *                                  completion.
 * @param last_op               1 if this is the final message in the
 *                                  transaction.
 *
 * @return                      0 on success;
 *                              HAL_I2C_ERR_[...] code on failure.
 */
int i2cn_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                      os_time_t timeout, uint8_t last_op, int retries);

/**
 * @brief Writes to and then read from an I2C slave, retrying the specified
 * number of times on failure.
 *
 * This should be used whenever sequence of write (e.g. register address on
 * sensor) and read (e.g. data from that register) is required in favor of
 * separate write and read as it guaranteed that in case of failed read, write
 * will be performed once more to guarantee read is handled properly by device.
 *
 * @param i2c_num               The index of the I2C interface to write to.
 * @param wdata                 Additional parameters describing the write
 *                                  operation.
 * @param rdata                 Additional parameters describing the read
 *                                  operation.
 * @param timeout               The time, in OS ticks, to wait for either read
 *                                  or write operation completion.
 * @param last_op               1 if read is the final message in the
 *                                  transaction.
 *
 * @return                      0 on success;
 *                              HAL_I2C_ERR_[...] code on failure.
 */
int i2cn_master_write_read_transact(uint8_t i2c_num,
                                    struct hal_i2c_master_data *wdata,
                                    struct hal_i2c_master_data *rdata,
                                    os_time_t timeout, uint8_t last_op,
                                    int retries);

#ifdef __cplusplus
}
#endif

#endif
