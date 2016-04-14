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

#ifndef HAL_I2C_H
#define HAL_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <bsp/bsp_sysid.h>

/* This is the API for an i2c bus.  I tried to make this a simple API */

struct hal_i2c;

/* when sending a packet, use this structure to pass the arguments */
struct hal_i2c_master_data {
    uint8_t  address;   /* destination address */
            /* NOTE: Write addresses are even and read addresses are odd
             * but in this API, you must enter a single address that is the
             * write address divide by 2.  For example, for address 
             * 80, you would enter a 40 */
    uint8_t *buffer;    /* buffer space to hold the transmit or receive */
    uint16_t len;       /* length of buffer to transmit or receive */
};

/* Initialize a new i2c device with the given system id.
 * Returns negative on error, 0 on success. */
struct hal_i2c*
hal_i2c_init(enum system_device_id sysid);

/* Sends <len> bytes of data on the i2c.  This API assumes that you 
 * issued a successful start condition.  It will fail if you
 * have not. This API does NOT issue a stop condition.  You must stop the 
 * bus after successful or unsuccessful write attempts.  Returns 0 on 
 * success, negative on failure */
int
hal_i2c_master_write(struct hal_i2c*, struct hal_i2c_master_data *pdata);

/* Reads <len> bytes of data on the i2c.  This API assumes that you 
 * issued a successful start condition.  It will fail if you
 * have not. This API does NOT issue a stop condition.  You must stop the 
 * bus after successful or unsuccessful write attempts.  Returns 0 on 
 * success, negative on failure */
int
hal_i2c_master_read(struct hal_i2c*, struct hal_i2c_master_data *pdata);

/* issues a start condition and address on the SPI bus. Returns 0 
 * on success, negative on error.
 */
int 
hal_i2c_master_start(struct hal_i2c*);

/* issues a stop condition on the bus.  You must issue a stop condition for
 * every successful start condition on the bus */
int 
hal_i2c_master_stop(struct hal_i2c*);


/* Probes the i2c bus for a device with this address.  THIS API 
 * issues a start condition, probes the address and issues a stop
 * condition.  
 */
int 
hal_i2c_master_probe(struct hal_i2c*, uint8_t address);

#ifdef __cplusplus
}
#endif

#endif /* HAL_I2C_H */
