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

#ifndef _SAMD21_HAL_PWM_H__
#define _SAMD21_HAL_PWM_H__

/* this is where the pin definitions are */
#include "src/sam0/utils/header_files/io.h"

/* the Samd parts have two different devices capable of doing PWM
 * There are two separate drivers in the atmel source, so we 
 * create two different types */

enum samd_tc_device_id {
    SAMD_TC_DEV_3 = 0,
    SAMD_TC_DEV_4,
    SAMD_TC_DEV_5,
};

enum samd_tc_tcc_clock_prescaler {
    /** Divide clock by 1 */
    SAMD_TC_CLOCK_PRESCALER_DIV1,
    SAMD_TC_CLOCK_PRESCALER_DIV2,
    SAMD_TC_CLOCK_PRESCALER_DIV4,
    SAMD_TC_CLOCK_PRESCALER_DIV8,
    SAMD_TC_CLOCK_PRESCALER_DIV16,
    SAMD_TC_CLOCK_PRESCALER_DIV64,
    SAMD_TC_CLOCK_PRESCALER_DIV256,
    SAMD_TC_CLOCK_PRESCALER_DIV1024,
};

struct samd21_pwm_tc_config {
    enum samd_tc_tcc_clock_prescaler prescalar;
    int clock_freq;
};

/* This creates a new PWM object based on the samd21 TC devices */
struct hal_pwm *samd21_pwm_tc_create(enum samd_tc_device_id id, int channel, int pin,
                                     const struct samd21_pwm_tc_config *pconfig);


enum samd_tcc_device_id {
    SAMD_TCC_DEV_0 = 0,
    SAMD_TCC_DEV_1,
    SAMD_TCC_DEV_2,
};

struct samd21_pwm_tcc_config {
    enum samd_tc_tcc_clock_prescaler prescalar;
    int clock_freq;
};


struct hal_pwm *samd21_pwm_tcc_create(enum samd_tcc_device_id id, int channel, int pin,
                                      const struct samd21_pwm_tcc_config *pconfig);


#endif /* _SAMD21_HAL_PWM_H__ */
