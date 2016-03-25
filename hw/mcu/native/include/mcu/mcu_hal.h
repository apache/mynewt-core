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

/* This file defines the HAL implementations within this MCU */

#ifndef MCU_HAL_H
#define MCU_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <hal/hal_adc.h>
#include <mcu/mcu_devid.h>
    
/* This creates a new ADC object for this ADC source */
struct hal_adc_device_s * 
native_adc_random_create(enum McuDeviceDescriptor devid);

/* This creates a new ADC object for this ADC source */
struct hal_adc_device_s * 
native_adc_mmm_create(enum McuDeviceDescriptor devid);

/* This creates a new ADC object for this ADC source */
struct hal_adc_device_s * 
native_adc_file_create(enum McuDeviceDescriptor devid, char *fname);


#ifdef __cplusplus
}
#endif

#endif /* MCU_HAL_H */

