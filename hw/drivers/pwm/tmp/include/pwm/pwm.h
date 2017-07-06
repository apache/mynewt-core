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

#ifndef __PWM_H__
#define __PWM_H__

#include <os/os_dev.h>

#ifdef __cplusplus
extern "C" {
#endif

struct pwm_dev;

/**
 * Configure an ADC channel for this ADC device.  This is implemented
 * by the HW specific drivers.
 *
 * @param The ADC device to configure
 * @param The channel number to configure
 * @param An opaque blob containing HW specific configuration.
 *
 * @return 0 on success, non-zero error code on failure.
 */
typedef int (*pwm_configure_channel_func_t)(struct pwm_dev *dev, uint8_t, void *);

typedef int (*pwm_set_duty_cycle_func_t)(struct pwm_dev *dev, uint8_t, void *);

struct pwm_driver_funcs {
    pwm_configure_channel_func_t pwm_configure_channel;
    pwm_set_duty_cycle_func_t pwm_set_duty_cycle;
};

struct pwm_dev_config {
};

struct pwm_dev {
    struct os_dev pwm_dev;
    struct os_mutex pwm_lock;
    struct pwm_driver_funcs pwm_funcs;
    struct pwm_chan_config *pwm_chans;
    int pwm_chan_count;
};

int pwm_chan_config(struct pwm_dev *, uint8_t, void *);



#ifdef __cplusplus
}
#endif

#endif /* __PWM_H__ */
