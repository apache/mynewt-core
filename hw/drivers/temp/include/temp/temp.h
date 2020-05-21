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

#ifndef _TEMP_TEMP_H
#define _TEMP_TEMP_H

#include <stdint.h>
#include <os/os_dev.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Temperature in centi-degrees Celsius
 */
typedef int16_t temperature_t;

struct temperature_dev;

/**
 * This callback is called when the temperature is sampled. The callback
 * could be called from an interrupt context. Use temp_get_last_sample() to
 * get the sampled temperature.
 *
 * @param temp_dev The temperature device
 * @param arg The argument as passed to temp_set_callback
 *
 * @return 0 on success, non-zero error code on failure
 */
typedef void (*temperature_cb)(struct temperature_dev *temp_dev, void *arg);

typedef int (*temp_sample_func_t)(struct temperature_dev *);

struct temp_driver_funcs {
    temp_sample_func_t temp_sample;
};

struct temperature_dev {
    struct os_dev dev;
    struct temp_driver_funcs temp_funcs;
    temperature_t last_temp;
    temperature_cb callback;
    void *callback_arg;
};

/**
 * Set a function to be called when the temperature is sampled. The callback
 * could be called from an interrupt context.
 *
 * @param temp_dev The temperature device to set the callback for
 * @param callback The callback for when the temperature is read
 * @param arg The argument for the callback
 *
 * @return 0 on success, non-zero error code on failure
 */
int temp_set_callback(struct temperature_dev *temp_dev, temperature_cb callback, void *arg);

/**
 * Start sampling the temperature. This is implemented by the HW specific
 * drivers.
 *
 * @param temp_dev The temperature device
 *
 * @return 0 on success, non-zero error code on failure
 */
int temp_sample(struct temperature_dev *temp_dev);

/**
 * Returns the temperature measured by the last sampling. This a fast and safe
 * method.
 *
 * @param temp_dev The temperature device
 *
 * @return temperature during last sample
 */
temperature_t temp_get_last_sample(struct temperature_dev *temp_dev);

/**
 * INTERNAL
 * Use by the driver to indicate that it is done sampling.
 *
 * @param temp_dev The temperature device
 * @param sample The measured temperature
 *
 */
void temp_sample_completed(struct temperature_dev *temp_dev, temperature_t sample);


#ifdef __cplusplus
}
#endif

#endif /* _TEMP_TEMP_H */
