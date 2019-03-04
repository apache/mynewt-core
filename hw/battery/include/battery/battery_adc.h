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

#ifndef __BATTERY_ADC_H__
#define __BATTERY_ADC_H__

#include <os/mynewt.h>
#include <battery/battery_drv.h>
#include <battery/battery.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Structure passed to os_dev_create
 */
struct battery_adc_cfg
{
    struct os_dev *battery;
    const char *adc_dev_name;
    /* Platform dependent configuration, needed for os_dev_open() of adc device */
    void *adc_open_arg;
    /* Platform dependent channel configuration, needed for adc_chan_config() */
    void *adc_channel_cfg;
    /* channel */
    uint8_t channel;
    /* multiplier for ADC reading */
    int mul;
    /* divider for ADC reading */
    int div;
    /* GPIO pin to activate for measurement */
    int activation_pin;
    /* GPIO activation needed for measurement */
    uint8_t activation_pin_needed:1;
    /* GPIO value needed for measurement */
    uint8_t activation_pin_level:1;
};

/* battery_adc device */
struct battery_adc
{
    /* Underlying OS device */
    struct battery_driver dev;

    /* Configuration values */
    struct battery_adc_cfg cfg;

    struct adc_dev *adc_dev;
};

/**
 * Expects to be called back through os_dev_create().
 *
 * @param dec  ptr to the device object associated with this ADC channel
 * @param arg  argument passed to OS device init
 *
 * @return 0 on success, non-zero on failure.
 */
int battery_adc_init(struct os_dev *dev, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* __BATTERY_ADC_H__ */
