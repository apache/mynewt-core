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

#include <stdio.h>
#include <hal/hal_i2c.h>

int
hal_i2c_init(uint8_t i2c_num, void *cfg)
{
    return 0;
}

int
hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                     uint32_t timeout, uint8_t last_op)
{
    uint16_t i;

    printf("hal_i2c wrote %d byte(s) on device 0x%02X (I2C_%d):",
           pdata->len, pdata->address, i2c_num);
    for (i=0; i<pdata->len; i++) {
        printf(" %02X", pdata->buffer[i]);
    }
    printf("\n");
    fflush(stdout);
    return 0;
}

int
hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                    uint32_t timeout, uint8_t last_op)
{
    uint16_t i;

    printf("hal_i2c read  %d byte(s) on device 0x%02X (I2C_%d):",
           pdata->len, pdata->address, i2c_num);
    for (i=0; i<pdata->len; i++) {
        printf(" %02X", pdata->buffer[i]);
    }
    printf("\n");
    fflush(stdout);

    return 0;
}

int
hal_i2c_master_probe(uint8_t i2c_num, uint8_t address,
                     uint32_t timeout)
{
    int val;

    /* ToDo: return '0' on any I2C address(es) you wish to match */
    switch (address) {
        default:
          val = -1;
    }

    return val;
}
