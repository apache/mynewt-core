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
#include <bsp/bsp_sysid.h>
#include <hal/hal_i2c.h>
#include <hal/hal_i2c_int.h>

struct hal_i2c *
hal_i2c_init(enum system_device_id sysid)
{
    return bsp_get_hal_i2c_driver(sysid);
}

int
hal_i2c_master_write(struct hal_i2c *pi2c, struct hal_i2c_master_data *ppkt)
{
    if (pi2c && pi2c->driver_api && pi2c->driver_api->hi2cm_write_data )
    {
        return pi2c->driver_api->hi2cm_write_data(pi2c, ppkt);
    }
    return -1;
}

int
hal_i2c_master_read(struct hal_i2c *pi2c, struct hal_i2c_master_data *ppkt)
{
    if (pi2c && pi2c->driver_api && pi2c->driver_api->hi2cm_read_data )
    {
        return pi2c->driver_api->hi2cm_read_data(pi2c, ppkt);
    }
    return -1;
}

int
hal_i2c_master_probe(struct hal_i2c *pi2c, uint8_t address)
{
    if (pi2c && pi2c->driver_api && pi2c->driver_api->hi2cm_probe )
    {
        return pi2c->driver_api->hi2cm_probe(pi2c, address);
    }
    return -1;
}

int
hal_i2c_master_begin(struct hal_i2c *pi2c)
{
    if (pi2c && pi2c->driver_api && pi2c->driver_api->hi2cm_start )
    {
        return pi2c->driver_api->hi2cm_start(pi2c);
    }
    return -1;
}

int
hal_i2c_master_end(struct hal_i2c *pi2c)
{
    if (pi2c && pi2c->driver_api && pi2c->driver_api->hi2cm_stop )
    {
        return pi2c->driver_api->hi2cm_stop(pi2c);
    }
    return -1;
}
