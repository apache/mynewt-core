/*
 * Copyright 2020 Casper Meijn <casper@meijn.net>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "sgm4056/sgm4056.h"
#include "hal/hal_gpio.h"

int
sgm4056_dev_init(struct os_dev *odev, void *arg)
{
    struct sgm4056_dev *dev = (struct sgm4056_dev *)odev;
    const struct sgm4056_dev_config *cfg = arg;
    int rc = 0;

    if (!dev) {
        rc = SYS_ENODEV;
        goto err;
    }

    dev->config = *cfg;

    rc = hal_gpio_init_in(dev->config.power_presence_pin, HAL_GPIO_PULL_NONE);
    if (rc) {
        goto err;
    }

    rc = hal_gpio_init_in(dev->config.charge_indicator_pin, HAL_GPIO_PULL_NONE);
    if (rc) {
        goto err;
    }

err:
    return rc;
}

int
sgm4056_get_power_presence(struct sgm4056_dev *dev, int *power_present)
{
    int gpio_value = hal_gpio_read(dev->config.power_presence_pin);

    *power_present = (gpio_value == 0);
    return 0;
}

int
sgm4056_get_charge_indicator(struct sgm4056_dev *dev, int *charging)
{
    int gpio_value = hal_gpio_read(dev->config.charge_indicator_pin);

    *charging = (gpio_value == 0);
    return 0;
}

int
sgm4056_get_charger_status(struct sgm4056_dev *dev,
                           charge_control_status_t *charger_status)
{
    int power_present;
    int charging;
    int rc;

    rc = sgm4056_get_power_presence(dev, &power_present);
    if (rc) {
        goto err;
    }

    rc = sgm4056_get_charge_indicator(dev, &charging);
    if (rc) {
        goto err;
    }

    if (!power_present) {
        *charger_status = CHARGE_CONTROL_STATUS_NO_SOURCE;
    } else {
        if (charging) {
            *charger_status = CHARGE_CONTROL_STATUS_CHARGING;
        } else {
            *charger_status = CHARGE_CONTROL_STATUS_CHARGE_COMPLETE;
        }
    }

err:
    return rc;
}
