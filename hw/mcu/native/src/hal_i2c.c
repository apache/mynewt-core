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
#include "os/mynewt.h"
#include "hal/hal_i2c.h"
#include "mcu/mcu_sim_i2c.h"

struct {
    struct os_mutex mgr_lock;
    SLIST_HEAD(, hal_i2c_sim_driver) mgr_sim_list;
} hal_i2c_sim_mgr;

int
hal_i2c_init(uint8_t i2c_num, void *cfg)
{
    os_mutex_init(&hal_i2c_sim_mgr.mgr_lock);

    return 0;
}

int
hal_i2c_master_write(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                     uint32_t timeout, uint8_t last_op)
{
    struct hal_i2c_sim_driver *cursor;

    cursor = NULL;
    SLIST_FOREACH(cursor, &hal_i2c_sim_mgr.mgr_sim_list, s_next) {
        if (cursor->addr == pdata->address) {
            /* Forward the read request to the sim driver */
            return cursor->sd_write(i2c_num, pdata, timeout, last_op);
        }
    }

    return 0;
}

int
hal_i2c_master_read(uint8_t i2c_num, struct hal_i2c_master_data *pdata,
                    uint32_t timeout, uint8_t last_op)
{
    struct hal_i2c_sim_driver *cursor;

    cursor = NULL;
    SLIST_FOREACH(cursor, &hal_i2c_sim_mgr.mgr_sim_list, s_next) {
        if (cursor->addr == pdata->address) {
            /* Forward the read request to the sim driver */
            return cursor->sd_read(i2c_num, pdata, timeout, last_op);
        }
    }

    return 0;
}

int
hal_i2c_master_probe(uint8_t i2c_num, uint8_t address,
                     uint32_t timeout)
{
    struct hal_i2c_sim_driver *cursor;

    cursor = NULL;
    SLIST_FOREACH(cursor, &hal_i2c_sim_mgr.mgr_sim_list, s_next) {
        if (cursor->addr == address) {
            return 0;
        }
    }

    return -1;
}

/**
 * Lock sim manager to access the list of simulated drivers
 */
int
hal_i2c_sim_mgr_lock(void)
{
    int rc;

    rc = os_mutex_pend(&hal_i2c_sim_mgr.mgr_lock, OS_TIMEOUT_NEVER);
    if (rc == 0 || rc == OS_NOT_STARTED) {
        return (0);
    }
    return (rc);
}

/**
 * Unlock sim manager once the list of simulated drivers has been accessed
 */
void
hal_i2c_sim_mgr_unlock(void)
{
    (void) os_mutex_release(&hal_i2c_sim_mgr.mgr_lock);
}

/**
 * Insert a simulated driver into the list
 */
static void
hal_i2c_sim_mgr_insert(struct hal_i2c_sim_driver *driver)
{
    SLIST_INSERT_HEAD(&hal_i2c_sim_mgr.mgr_sim_list, driver, s_next);
}

int
hal_i2c_sim_register(struct hal_i2c_sim_driver *drv)
{
    int rc;

    rc = hal_i2c_sim_mgr_lock();
    if (rc != 0) {
        goto err;
    }

    printf("Registering I2C sim driver for 0x%02X\n", drv->addr);
    fflush(stdout);
    hal_i2c_sim_mgr_insert(drv);

    hal_i2c_sim_mgr_unlock();

    return (0);
err:
    return (rc);
}
