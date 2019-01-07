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

#include "hal/hal_i2c.h"
#include "i2cn/i2cn.h"

int
i2cn_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                 os_time_t timeout, uint8_t last_op, int retries)
{
    int rc = 0;
    int i;

    /* Ensure at least one try. */
    if (retries < 0) {
        retries = 0;
    }

    for (i = 0; i <= retries; i++) {
        rc = hal_i2c_master_read(i2c_num, pdata, timeout, last_op);
        if (rc == 0) {
            break;
        }
    }

    return rc;
}

int
i2cn_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                  os_time_t timeout, uint8_t last_op, int retries)
{
    int rc = 0;
    int i;

    /* Ensure at least one try. */
    if (retries < 0) {
        retries = 0;
    }

    for (i = 0; i <= retries; i++) {
        rc = hal_i2c_master_write(i2c_num, pdata, timeout, last_op);
        if (rc == 0) {
            break;
        }
    }

    return rc;
}

int
i2cn_master_write_read_transact(uint8_t i2c_num,
                                struct hal_i2c_master_data *wdata,
                                struct hal_i2c_master_data *rdata,
                                os_time_t timeout, uint8_t last_op, int retries)
{
    int rc = 0;
    int i;

    /* Ensure at least one try. */
    if (retries < 0) {
        retries = 0;
    }

    for (i = 0; i <= retries; i++) {
        rc = hal_i2c_master_write(i2c_num, wdata, timeout, 0);
        if (rc != 0) {
            continue;
        }

        rc = hal_i2c_master_read(i2c_num, rdata, timeout, last_op);
        if (rc == 0) {
            break;
        }
    }

    return rc;

}
