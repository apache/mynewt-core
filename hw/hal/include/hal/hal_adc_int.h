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

#ifndef HAL_ADC_INT_H
#define HAL_ADC_INT_H

#include <inttypes.h>


struct hal_adc_device_s;

/* These functions make up the driver API for ADC devices.  All 
 * ADC devices with Mynewt support implement this interface */
struct hal_adc_funcs_s {
    int (*hadc_read)                     (const struct hal_adc_device_s *padc);
    int  (*hadc_get_resolution)          (const struct hal_adc_device_s *padc);
    int (*hadc_get_reference_mvolts)     (const struct hal_adc_device_s *padc);
    int  (*hadc_init)                    (const struct hal_adc_device_s *padc);
};

/* This is the internal device representation for a hal_adc device.
 * 
 *  driver_api -- the function pointers for the driver associated this device
 *  device_id -- many drivers support multiple devices. This enumerates them
 *  _reserved -- pad 64 bits and reserve space 
 * 
 */
struct hal_adc_device_s {
    const struct hal_adc_funcs_s  *driver_api;
    uint16_t                       device_id;
    uint16_t                       _reserved;
};

#endif /* HAL_ADC_INT_H */