/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * resarding copyright ownership.  The ASF licenses this file
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

#include <assert.h>
#include <string.h>

#include "defs/error.h"
#include "hal/hal_gpio.h"
#include "bq24040/bq24040.h"
#include "console/console.h"
#include "log/log.h"

/* Exports for the sensor API */
static int bq24040_sensor_read(struct sensor *, sensor_type_t,
     sensor_data_func_t, void *, uint32_t);
static int bq24040_sensor_get_config(struct sensor *, sensor_type_t,
     struct sensor_cfg *);
static int bq24040_sensor_set_config(struct sensor *, void *);

static const struct sensor_driver g_bq24040_sensor_driver = {
    .sd_read = bq24040_sensor_read,
    .sd_get_config = bq24040_sensor_get_config,
    .sd_set_config = bq24040_sensor_set_config,
};

int
bq24040_init(struct os_dev *dev, void *arg)
{
    struct bq24040 *bq24040;
    struct sensor *sensor;
    int rc;

    if (!dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    bq24040 = (struct bq24040 *)dev;

    /* register a log */
    // ...

    sensor = &bq24040->sensor;

    /* Initialize the stats entry */
    // ...

    /* Register the entry with the stats registery */
    // ...

    rc = sensor_init(sensor, dev);
    if (rc) {
        goto err;
    }

    /* Add the driver with all the supported types */
    rc = sensor_set_driver(sensor, SENSOR_TYPE_USER_DEFINED_1 | 
            SENSOR_TYPE_USER_DEFINED_2,
            (struct sensor_driver *)&g_bq24040_sensor_driver);
    if (rc) {
        goto err;
    }

    rc = sensor_mgr_register(sensor);
    if (rc) {
        goto err;
    }

    return 0;
err:
    return rc;
}

int
bq24040_config(struct bq24040 *bq24040, struct bq24040_cfg *cfg)
{
    int rc;

    rc = sensor_set_type_mask(&(bq24040->sensor), cfg->s_mask);
    if (rc) {
        goto err;
    }

    bq24040->cfg = *cfg;

    if(cfg->pg_pin_num != -1)
    {
        hal_gpio_init_in(cfg->pg_pin_num, HAL_GPIO_PULL_NONE);
    }
    if(cfg->chg_pin_num != -1)
    {
        hal_gpio_init_in(cfg->chg_pin_num, HAL_GPIO_PULL_NONE);
    }
    if(cfg->ts_pin_num != -1)
    {
        hal_gpio_init_out(cfg->ts_pin_num, 0);
    }
    if(cfg->iset2_pin_num != -1)
    {
        hal_gpio_init_out(cfg->iset2_pin_num, 1);
    }

    return 0;
err:
    return (rc);
}

int
bq24040_get_charge_source_status(struct bq24040 *bq24040, int *status)
{
    if(bq24040->cfg.pg_pin_num != -1)
    {
        *status = hal_gpio_read(bq24040->cfg.pg_pin_num) == 0 ? 1 : 0;
        return 0;
    }
    return 1;
}

int
bq24040_get_charging_status(struct bq24040 *bq24040, int *status)
{
    if(bq24040->cfg.chg_pin_num != -1)
    {
        *status = hal_gpio_read(bq24040->cfg.chg_pin_num) == 0 ? 1 : 0;
        return 0;
    }
    return 1;
}

int
bq24040_enable(struct bq24040 *bq24040)
{
    if(bq24040->cfg.ts_pin_num != -1)
    {
        hal_gpio_write(bq24040->cfg.ts_pin_num, 1);
        return 0;
    }
    return SYS_EINVAL;
}

int
bq24040_disable(struct bq24040 *bq24040)
{
    if(bq24040->cfg.ts_pin_num != -1)
    {
        hal_gpio_write(bq24040->cfg.ts_pin_num, 0);
        return 0;
    }
    return SYS_EINVAL;
}

/**
 *  Reads the charge source and charging status pins (if configured)
 *
 * @param The bq24040 charge controller to read from
 * @param The type(s) of sensor values to read.  Mask containing that type, provide
 *        all, to get all values.
 * @param The function to call with each value read.  If NULL, it calls all
 *        sensor listeners associated with this function.
 * @param The argument to pass to the read callback.
 * @param Timeout.  If block until result, specify OS_TIMEOUT_NEVER, 0 returns
 *        immediately (no wait.)
 *
 * @return 0 on success, non-zero error code on failure.
 */
static int
bq24040_sensor_read(struct sensor *sns, sensor_type_t type,
     sensor_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int charge_source_status;
    int charging_status;
    struct bq24040 *bq24040;
    int rc;

    if (!(type & SENSOR_TYPE_USER_DEFINED_1) &&
        !(type & SENSOR_TYPE_USER_DEFINED_2)) {
        rc = SYS_EINVAL;
        goto err;
    }

    bq24040 = (struct bq24040 *)SENSOR_GET_DEVICE(sns);

    charge_source_status = 0;
    charging_status = 0;

    if (type & SENSOR_TYPE_USER_DEFINED_1)
    {
        rc = bq24040_get_charge_source_status(bq24040,
                &charge_source_status);
        if (rc) {
            goto err;
        }

        rc = data_func(sns, data_arg, &charge_source_status, 
                SENSOR_TYPE_USER_DEFINED_1);
        if (rc) {
            goto err;
        }
    }

    if (type & SENSOR_TYPE_USER_DEFINED_2)
    {
        rc = bq24040_get_charging_status(bq24040,
                &charging_status);
        if (rc) {
            goto err;
        }

        rc = data_func(sns, data_arg, &charging_status,
                SENSOR_TYPE_USER_DEFINED_2);
        if (rc) {
            goto err;
        }
    }

    return 0;
err:
    return rc;
}

/**
 * Get the configuration of the bq24040.
 *
 * @param The type of sensor value to get configuration for
 * @param A pointer to the config structure to place the returned result into.
 *
 * @return 0 on success, non-zero error code on failure.
 */
static int
bq24040_sensor_get_config(struct sensor *sns, sensor_type_t type,
     struct sensor_cfg * cfg)
{
    int rc;

    if (!(type & SENSOR_TYPE_USER_DEFINED_1) ||
        !(type & SENSOR_TYPE_USER_DEFINED_2)) {
        rc = SYS_EINVAL;
        goto err;
    }

    cfg->sc_valtype = SENSOR_VALUE_TYPE_INT32;

    return 0;
err:
    return rc;
}

static int
bq24040_sensor_set_config(struct sensor *sns, void *cfg)
{
    struct bq24040* bq24040 = (struct bq24040 *)SENSOR_GET_DEVICE(sns);
    
    return bq24040_config(bq24040, (struct bq24040_cfg*)cfg);
}
