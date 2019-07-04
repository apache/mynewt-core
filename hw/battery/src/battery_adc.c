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

#include "os/mynewt.h"
#include <battery/battery.h>
#include <battery/battery_prop.h>
#include <battery/battery_drv.h>
#include <battery/battery_adc.h>
#include <adc/adc.h>
#include <hal/hal_gpio.h>

/* Battery manager interface functions */

static int
battery_adc_property_get(struct battery_driver *driver,
                         struct battery_property *property, uint32_t timeout)
{
    int rc = 0;
    struct battery_adc *bat_adc = (struct battery_adc *)driver->bd_driver_data;
    int val;

    /* Only one property is supported, voltage */
    if (property->bp_type == BATTERY_PROP_VOLTAGE_NOW &&
        property->bp_flags == 0) {

        /* Activate GPIO if it was configured */
        if (bat_adc->cfg.activation_pin_needed &&
            bat_adc->cfg.activation_pin != -1) {
            hal_gpio_write(bat_adc->cfg.activation_pin,
                    bat_adc->cfg.activation_pin_level);
        }
        /* Blocking read of voltage */
        rc = adc_read_channel(bat_adc->adc_dev, bat_adc->cfg.channel, &val);

        /* Deactivate GPIO if it was configured */
        if (bat_adc->cfg.activation_pin_needed &&
            bat_adc->cfg.activation_pin != -1) {
            hal_gpio_write(bat_adc->cfg.activation_pin,
                    bat_adc->cfg.activation_pin_level ? 0 : 1);
        }

        if (rc == 0) {
            property->bp_valid = 1;
            /* Convert value to according to reference voltage and multiplier
             * and divider if external resistor divider is used for measurement.
             */
            property->bp_value.bpv_voltage =
                    adc_result_mv(bat_adc->adc_dev, bat_adc->cfg.channel, val) *
                    bat_adc->cfg.mul / bat_adc->cfg.div;
        }
    } else {
        rc = -1;
        assert(0);
    }

    return rc;
}

static int
battery_adc_property_set(struct battery_driver *driver,
                         struct battery_property *property)
{
    int rc = -1;

    return rc;
}

static int
battery_adc_enable(struct battery *battery)
{
    return 0;
}

static int
battery_adc_disable(struct battery *battery)
{
    return 0;
}

static const struct battery_driver_functions battery_adc_drv_funcs = {
    .bdf_property_get     = battery_adc_property_get,
    .bdf_property_set     = battery_adc_property_set,

    .bdf_enable           = battery_adc_enable,
    .bdf_disable          = battery_adc_disable,
};

static const struct battery_driver_property battery_adc_properties[] = {
    { BATTERY_PROP_VOLTAGE_NOW, 0, "VoltageADC" },
    { BATTERY_PROP_NONE },
};

static int
battery_adc_open(struct os_dev *dev, uint32_t timeout, void *arg)
{
    int rc = -1;
    struct battery_adc *bat_adc = (struct battery_adc *)dev;
    (void)arg;

    /* Open ADC with parameters specified in BSP */
    bat_adc->adc_dev = (struct adc_dev *)os_dev_open(
            (char *)bat_adc->cfg.adc_dev_name, timeout, bat_adc->cfg.adc_open_arg);
    if (bat_adc->adc_dev) {
        /* Setup channel configuration to use for battery voltage */
        adc_chan_config(bat_adc->adc_dev, bat_adc->cfg.channel,
                bat_adc->cfg.adc_channel_cfg);

        /* Additional GPIO needed before measurement ? */
        if (bat_adc->cfg.activation_pin_needed &&
            bat_adc->cfg.activation_pin != -1) {
            hal_gpio_init_out(bat_adc->cfg.activation_pin,
                        bat_adc->cfg.activation_pin_level ? 0 : 1);
        }
        rc = 0;
    }
    return rc;
}

static int
battery_adc_close(struct os_dev *dev)
{
    struct battery_adc *bat_adc = (struct battery_adc *)dev;
    if (bat_adc->adc_dev) {
        os_dev_close((struct os_dev *)&bat_adc->adc_dev);
        bat_adc->adc_dev = NULL;
    }
    return 0;
}

/* driver open implementation */
int
battery_adc_init(struct os_dev *dev, void *arg)
{
    struct battery_adc *bat_adc;
    struct battery_adc_cfg *init_arg = (struct battery_adc_cfg *)arg;

    if (!dev || !arg) {
        return SYS_ENODEV;
    }

    OS_DEV_SETHANDLERS(dev, battery_adc_open, battery_adc_close);

    bat_adc = (struct  battery_adc *)dev;
    bat_adc->cfg = *((struct battery_adc_cfg *)arg);
    bat_adc->dev.bd_funcs = &battery_adc_drv_funcs;
    bat_adc->dev.bd_driver_data = bat_adc;
    bat_adc->dev.bd_driver_properties = battery_adc_properties;
    /* Divider and multiplier are set to 1 by default just make sure they
     * are not set to 0
     */
    assert(bat_adc->cfg.div != 0);
    assert(bat_adc->cfg.mul != 0);
    /* Add driver to battery, this will update battery properties */
    battery_add_driver(init_arg->battery, &bat_adc->dev);

    return 0;
}

