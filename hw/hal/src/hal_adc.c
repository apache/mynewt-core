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
#include <inttypes.h>
#include <hal/hal_adc.h>
#include <hal/hal_adc_int.h>

struct hal_adc_device_s *
hal_adc_init(enum SystemDeviceDescriptor pin) 
{
    return bsp_get_hal_adc_device(pin);
}

int 
hal_adc_read(struct hal_adc_device_s *padc) 
{
    if(padc && padc->driver_api && padc->driver_api->hadc_read) {
        return padc->driver_api->hadc_read(padc);
    }
    return -1;
}

int 
hal_adc_get_resolution(struct hal_adc_device_s *padc) 
{
    if(padc && padc->driver_api && padc->driver_api->hadc_get_resolution) {
        return padc->driver_api->hadc_get_resolution(padc);
    }
    return -1;
}

int 
hal_adc_get_reference_voltage_mvolts(struct hal_adc_device_s *padc) 
{
    if(padc && padc->driver_api && padc->driver_api->hadc_get_reference_mvolts) {
        return padc->driver_api->hadc_get_reference_mvolts(padc);
    }
    return -1;
}

/* returns the ADC read value converted to mvolts or negative on error */
int 
hal_adc_val_convert_to_mvolts(struct hal_adc_device_s *padc, int val) 
{
    int ret_val = -1;
    
    if(val >= 0)  {
        int ref;
        ref = hal_adc_get_reference_voltage_mvolts(padc);   
        if(ref > 0) {
            val *= ref;
            ref = hal_adc_get_resolution(padc);            
            /* doubt there will be many 1 bit ADC, but this will
             * adjust if its two bits or more */
            if( ref > 1) {
                /* round */
                val += (1 << (ref - 2));
                ret_val = (val >> ref);                
            } 
        }
    }
    return ret_val;
}