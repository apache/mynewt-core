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

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <inttypes.h>
#include "hal/hal_flash_int.h"
#include "hal/hal_adc_int.h"
#include "mcu/native_bsp.h"

const struct hal_flash *
bsp_flash_dev(uint8_t id)
{
    /*
     * Just one to start with
     */
    if (id != 0) {
        return NULL;
    }
    return &native_flash_dev;
}
const struct hal_adc_device_s ADC0 = {
    &random_adc_funcs,
    0,
};

const struct hal_adc_device_s ADC1 = {
    &random_adc_funcs,
    1,
};

const struct hal_adc_device_s ADC2 = {
    &mmm_adc_funcs,
    0,
};

const struct hal_adc_device_s ADC3 = {
    &mmm_adc_funcs,
    1,
};

const struct hal_adc_device_s ADC4 = {
    &mmm_adc_funcs,
    2
};

const struct hal_adc_device_s ADC5 = {
    &file_adc_funcs,
    0,
};

const struct hal_adc_device_s *PADC0 = &ADC0;
const struct hal_adc_device_s *PADC1 = &ADC1;
const struct hal_adc_device_s *PADC2 = &ADC2;
const struct hal_adc_device_s *PADC3 = &ADC3;
const struct hal_adc_device_s *PADC4 = &ADC4;
const struct hal_adc_device_s *PADC5 = &ADC5;