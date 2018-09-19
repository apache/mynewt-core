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


#ifndef H_MCU_SIM_I2C_
#define H_MCU_SIM_I2C_

#include <inttypes.h>
#include <hal/hal_i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

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
typedef int (*hal_i2c_sim_master_write_t)(uint8_t i2c_num,
             struct hal_i2c_master_data *pdata, uint32_t timeout,
             uint8_t last_op);

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
typedef int (*hal_i2c_sim_master_read_t)(uint8_t i2c_num,
             struct hal_i2c_master_data *pdata,
             uint32_t timeout, uint8_t last_op);

struct hal_i2c_sim_driver {
    hal_i2c_sim_master_write_t sd_write;
    hal_i2c_sim_master_read_t sd_read;

    /* 7-bit I2C device address */
    uint8_t addr;

    /* Reserved for future use */
    uint8_t rsvd[3];

    /* The next simulated sensor in the global sim driver list. */
    SLIST_ENTRY(hal_i2c_sim_driver) s_next;
};

/**
* Register a driver simulator
*
* @param drv The simulated driver to register
*
* @return 0 on success, non-zero on failure.
*/
int hal_i2c_sim_register(struct hal_i2c_sim_driver *drv);

#ifdef __cplusplus
}
#endif

#endif /* H_MCU_SIM_I2C_ */
