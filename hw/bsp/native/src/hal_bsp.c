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

/* This BSP will use all the ADCs provided by the native MCU implementation */
extern int 
bsp_get_hal_adc_driver(int sysid,  
                       int *devid_out, 
                       struct hal_adc_s **padc_out) {
    switch(sysid) {
        case 0:
        case 1:
            *devid_out = sysid;
            *padc_out = &native_random_adc;
            return 0;
        case 2:
        case 3:
        case 4:
            *devid_out = (sysid - 2);
            *padc_out = &native_min_mid_max_adc;
            return 0;
        case 5:
            *devid_out = (sysid - 5);
            *padc_out = &native_file_adc;            
            return 0;
    }
    return -1;
}