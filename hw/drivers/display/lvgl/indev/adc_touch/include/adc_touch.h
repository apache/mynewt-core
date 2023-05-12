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

#ifndef ADC_TOUCH_H
#define ADC_TOUCH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void * adc_dev_t;

/**
 * Open ADC device for touch screen access
 *
 * Function should be implemented by user to setup ADC that will be used
 * to measure values connected to X and Y electrodes.
 *
 * @param x_pin - pin to use for X coordinate measurement.
 * @param y_pin - pit to use for Y coordinate measurement.
 * @return value to be used in subsequent adc_touch_adc_read function
 */
adc_dev_t adc_touch_adc_open(int x_pin, int y_pin);

/**
 * Read ADC value for given pin
 *
 * @param adc - value returned form adc_touch_adc_open()
 * @param pin - one of the pins to measure
 * @return value measured on selected pin
 */
uint16_t adc_touch_adc_read(adc_dev_t adc, int pin);

#ifdef __cplusplus
}
#endif

#endif /* ADC_TOUCH_H */
