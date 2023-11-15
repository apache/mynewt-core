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

#ifndef _SAMD21_HAL_I2C_H__
#define _SAMD21_HAL_I2C_H__

/* this is where the pin definitions are */
#include "src/sam0/utils/header_files/io.h"

enum samd21_i2c_device {
    SAMD21_I2C_SERCOM0,
    SAMD21_I2C_SERCOM1,
    SAMD21_I2C_SERCOM2,
    SAMD21_I2C_SERCOM3,
    SAMD21_I2C_SERCOM4,
    SAMD21_I2C_SERCOM5,
};

enum samd21_i2c_mux_setting {
    SAMD21_I2C_MUX_SETTING_C,
    SAMD21_I2C_MUX_SETTING_D,
};

/* These have to be set appropriately to be valid combination */
struct samd21_i2c_config {
    uint32_t pad0_pinmux;
    uint32_t pad1_pinmux;
    /* TODO baudrate and high speed */
};

/* This creates a new I2C object based on the samd21 TC devices */
struct hal_i2c *samd21_i2c_create(enum samd21_i2c_device dev_id,
                                  const struct samd21_i2c_config *pconfig);

#endif /* _SAMD21_HAL_I2C_H__ */
