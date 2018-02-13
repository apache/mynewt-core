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

/* Exports for the charge control API */
static int bq24040_chg_ctrl_read(struct charge_control *, charge_control_type_t,
        charge_control_data_func_t, void *, uint32_t);
static int bq24040_chg_ctrl_get_config(struct charge_control *, 
        charge_control_type_t, struct charge_control_cfg *);
static int bq24040_chg_ctrl_set_config(struct charge_control *, void *);
static int bq24040_chg_ctrl_enable(struct charge_control *);
static int bq24040_chg_ctrl_disable(struct charge_control *);

static const struct charge_control_driver g_bq24040_chg_ctrl_driver = {
    .ccd_read = bq24040_chg_ctrl_read,
    .ccd_get_config = bq24040_chg_ctrl_get_config,
    .ccd_set_config = bq24040_chg_ctrl_set_config,
    .ccd_enable = bq24040_chg_ctrl_enable,
    .ccd_disable = bq24040_chg_ctrl_disable,
};

static int bq24040_configure_pin(struct bq24040_pin *pin)
{
    if ((!pin) || (pin->bp_pin_num == -1))
    {
        return 0;
    }

    if (pin->bp_pin_direction == HAL_GPIO_MODE_IN)
    {
        if (pin->bp_irq_trig != HAL_GPIO_TRIG_NONE)
        {
            assert(pin->bp_irq_fn != NULL);
            return hal_gpio_irq_init(pin->bp_pin_num, pin->bp_irq_fn,
                    NULL, pin->bp_irq_trig, pin->bp_pull);
        }
        else
        {
            return hal_gpio_init_in(pin->bp_pin_num, pin->bp_pull);
        }
    }
    else if (pin->bp_pin_direction == HAL_GPIO_MODE_OUT)
    {
        return hal_gpio_init_out(pin->bp_pin_num, pin->bp_init_value);
    }
    
    return SYS_EINVAL;
}

int
bq24040_init(struct os_dev *dev, void *arg)
{
    struct bq24040 *bq24040;
    struct charge_control *cc;
    int rc;

    if (!dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    bq24040 = (struct bq24040 *)dev;

    /* register a log */
    // ...

    cc = &bq24040->b_chg_ctrl;

    /* Initialize the stats entry */
    // ...

    /* Register the entry with the stats registery */
    // ...

    rc = charge_control_init(cc, dev);
    if (rc) {
        goto err;
    }

    /* Add the driver with all the supported types */
    rc = charge_control_set_driver(cc, CHARGE_CONTROL_TYPE_STATUS |
            CHARGE_CONTROL_TYPE_FAULT,
            (struct charge_control_driver *)&g_bq24040_chg_ctrl_driver);
    if (rc) {
        goto err;
    }

    rc = charge_control_mgr_register(cc);
    if (rc) {
        goto err;
    }

    bq24040->b_is_enabled = false;

    return 0;
err:
    return rc;
}

int
bq24040_config(struct bq24040 *bq24040, struct bq24040_cfg *cfg)
{
    int rc;

    rc = charge_control_set_type_mask(&(bq24040->b_chg_ctrl), cfg->bc_mask);
    if (rc) {
        goto err;
    }

    bq24040->b_cfg = *cfg;

    rc = bq24040_configure_pin(cfg->bc_pg_pin);
    if (rc) {
        goto err;
    }

    rc = bq24040_configure_pin(cfg->bc_chg_pin);
    if (rc) {
        goto err;
    }
    
    rc = bq24040_configure_pin(cfg->bc_ts_pin);
    if (rc) {
        goto err;
    }
    
    rc = bq24040_configure_pin(cfg->bc_iset2_pin);
    if (rc) {
        goto err;
    } 

    return 0;
err:
    return (rc);
}

int
bq24040_get_charge_source_status(struct bq24040 *bq24040, int *status)
{
    int read_val;

    if (bq24040->b_cfg.bc_pg_pin->bp_pin_num != -1)
    {
        read_val = hal_gpio_read(bq24040->b_cfg.bc_pg_pin->bp_pin_num);
        *status = read_val == 0 ? 1 : 0;
        return 0;
    }
    return 1;
}

int
bq24040_get_charging_status(struct bq24040 *bq24040, int *status)
{
    int read_val;

    if (bq24040->b_cfg.bc_chg_pin->bp_pin_num != -1)
    {
        read_val = hal_gpio_read(bq24040->b_cfg.bc_chg_pin->bp_pin_num);
        *status = read_val == 0 ? 1 : 0;
        return 0;
    }
    return 1;
}

int
bq24040_enable(struct bq24040 *bq24040)
{
    if(bq24040->b_cfg.bc_ts_pin->bp_pin_num != -1)
    {
        hal_gpio_write(bq24040->b_cfg.bc_ts_pin->bp_pin_num, 1);
        bq24040->b_is_enabled = true;
        return 0;
    }
    return SYS_EINVAL;
}

int
bq24040_disable(struct bq24040 *bq24040)
{
    if(bq24040->b_cfg.bc_ts_pin->bp_pin_num != -1)
    {
        hal_gpio_write(bq24040->b_cfg.bc_ts_pin->bp_pin_num, 0);
        bq24040->b_is_enabled = false;
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
bq24040_chg_ctrl_read(struct charge_control *cc, charge_control_type_t type,
     charge_control_data_func_t data_func, void *data_arg, uint32_t timeout)
{
    int is_charge_source_present;
    int is_charging;
    int status;

    struct bq24040 *bq24040;
    int rc;

    if (!(type & CHARGE_CONTROL_TYPE_STATUS) &&
        !(type & CHARGE_CONTROL_TYPE_FAULT)) {
        rc = SYS_EINVAL;
        goto err;
    }

    bq24040 = (struct bq24040 *)CHARGE_CONTROL_GET_DEVICE(cc);

    is_charge_source_present = 0;
    is_charging = 0;
    status = CHARGE_CONTROL_STATUS_DISABLED;

    if (type & CHARGE_CONTROL_TYPE_STATUS)
    {
        if (!bq24040->b_is_enabled)
        {
            status = CHARGE_CONTROL_STATUS_DISABLED;
            goto done;
        }

        rc = bq24040_get_charge_source_status(bq24040, &is_charge_source_present);
        if (rc)
        {
            goto err;
        }

        rc = bq24040_get_charging_status(bq24040, &is_charging);
        if (rc)
        {
            goto err;
        }

        if (is_charge_source_present && is_charging)
        {
            status = CHARGE_CONTROL_STATUS_CHARGING;
        }
        else if (is_charge_source_present && !is_charging)
        {
            status = CHARGE_CONTROL_STATUS_CHARGE_COMPLETE;
        }
        else if (!is_charge_source_present)
        {
            status = CHARGE_CONTROL_STATUS_NO_SOURCE;
        }

        data_func(cc, data_arg, (void*)&status, CHARGE_CONTROL_TYPE_STATUS);

    }

    if (type & CHARGE_CONTROL_TYPE_FAULT)
    {
        // Get fault info
    }

done:
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
bq24040_chg_ctrl_get_config(struct charge_control *cc, 
        charge_control_type_t type, struct charge_control_cfg * cfg)
{
    int rc;

    if (!(type & CHARGE_CONTROL_TYPE_STATUS) ||
        !(type & CHARGE_CONTROL_TYPE_FAULT)) {
        rc = SYS_EINVAL;
        goto err;
    }

    return 0;
err:
    return rc;
}

static int
bq24040_chg_ctrl_set_config(struct charge_control *cc, void *cfg)
{
    struct bq24040* bq24040 = (struct bq24040 *)CHARGE_CONTROL_GET_DEVICE(cc);
    
    return bq24040_config(bq24040, (struct bq24040_cfg*)cfg);
}

static int
bq24040_chg_ctrl_enable(struct charge_control *cc)
{
    return 0;
}

static int
bq24040_chg_ctrl_disable(struct charge_control *cc)
{
    return 0;
}
