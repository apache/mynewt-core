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

struct hal_adc_funcs_s {
    int (*hadc_read)(int devid);
    int  (*hadc_get_resolution)(int devid);
    int (*hadc_get_reference_mvolts)(int devid);
    int  (*hadc_init)(int devid);
};

struct hal_adc_s {
    const struct hal_adc_funcs_s  *driver_api;
};

/* NOTE: When using the hal_adc APIs, your BSP will need to implement
 * this function.  This function maps the system ID to a device ID
 * and a driver interface for the underlying code to use. */
extern int 
bsp_get_hal_adc_driver(int sysid,  
                       int *devid_out, 
                       struct hal_adc_s **padc_out);

#endif /* HAL_ADC_INT_H */
