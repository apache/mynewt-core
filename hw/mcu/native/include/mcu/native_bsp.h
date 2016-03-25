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
#ifndef H_NATIVE_BSP_
#define H_NATIVE_BSP_

#include <stdio.h>

extern const struct hal_flash native_flash_dev;

/* this is a native build and these pins are not real.  They are used only
 * for simulated HAL devices */
enum McuPinDescriptor {
    MCU_PIN_0   = 0,
    MCU_PIN_1   = 1,
    MCU_PIN_2   = 2,
    MCU_PIN_3   = 3,
    MCU_PIN_4   = 4,
    MCU_PIN_5   = 5,
};


enum adc_channel_type {
    ADC_RANDOM,
    ADC_MIN,
    ADC_MID,
    ADC_MAX,
    ADC_FILE
};

struct hal_adc_device_s {
    enum adc_channel_type type;
    FILE *native_fs;
};

#endif /* H_NATIVE_BSP_ */
