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

#ifndef _SGM4056_H
#define _SGM4056_H

#include <os/os_dev.h>
#include <charge-control/charge_control.h>

struct sgm4056_dev_config {
    int power_presence_pin;
    int charge_indicator_pin;
};

struct sgm4056_dev {
    struct os_dev dev;
#if MYNEWT_VAL(SGM4056_USE_CHARGE_CONTROL)
    struct charge_control chg_ctrl;
    struct os_event interrupt_event;
#endif
    struct sgm4056_dev_config config;
};

/**
 * Init function for SGM4056 charger
 * @return 0 on success, non-zero on failure
 */
int sgm4056_dev_init(struct os_dev *dev, void *arg);

/**
 * Reads the state of the power presence indication. Value is 1 when the input
 * voltage is above the POR threshold but below the OVP threshold and 0 otherwise.
 *
 * @param dev The sgm4056 device to read on
 * @param power_present Power presence indication to be returned by the
 *      function (0 if input voltage is not detected, 1 if it is detected)
 *
 * @return 0 on success, non-zero on failure
 */
int sgm4056_get_power_presence(struct sgm4056_dev *dev, int *power_present);

/**
 * Reads the state of the charge indication. Value is 1 when a charge cycle
 * started and will be 0 when the end-of-charge (EOC) condition is met.
 *
 * @param dev The sgm4056 device to read on
 * @param charging Charge indication to be returned by the
 *      function (0 if battery is not charging, 1 if it is charging)
 *
 * @return 0 on success, non-zero on failure
 */
int sgm4056_get_charge_indicator(struct sgm4056_dev *dev, int *charging);

/**
 * Reads the state of the charger. This is a combination of the power presence
 * and charge indications. Value is either:
 * - CHARGE_CONTROL_STATUS_NO_SOURCE, when no power present
 * - CHARGE_CONTROL_STATUS_CHARGING, when power present and charging
 * - CHARGE_CONTROL_STATUS_CHARGE_COMPLETE, when power present and not charging
 *
 * @param dev The sgm4056 device to read on
 * @param charger_status Charger status indication to be returned
 *
 * @return 0 on success, non-zero on failure
 */
int sgm4056_get_charger_status(struct sgm4056_dev *dev, charge_control_status_t *charger_status);

#endif /* _SGM4056_H */
