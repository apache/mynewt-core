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
#include <assert.h>
#include <hal/hal_adc.h>
#include <hal/hal_adc_int.h>

int 
hal_adc_init(int sysid) 
{
    int devid;
    struct hal_adc_s *padc;
    int rc = bsp_get_hal_adc_driver(sysid, &devid, &padc);
    
    if(0 == rc) {
        return padc->driver_api->hadc_init(padc, devid);
    }
    return rc;
}

/*
 * read the ADC corresponding to sysid in your system.  Returns
 * the  adc value read or negative on error, See 
 * hal_adc_get_resolution to check the range of the return value */
int 
hal_adc_read(int sysid) 
{
    int devid;
    struct hal_adc_s *padc;
    int rc = bsp_get_hal_adc_driver(sysid, &devid, &padc);
    
    if(0 == rc) {
        return padc->driver_api->hadc_read(padc, devid);
    }
    return rc;
}

/* returns the number of bit of resolution in this ADC.  
 * For example if the system has an 8-bit ADC reporting 
 * values from 0= to 255 (2^8-1), this function would return
 * the value 8. */
int 
hal_adc_get_resolution(int sysid)  
{
    int devid;
    struct hal_adc_s *padc;
    int rc = bsp_get_hal_adc_driver(sysid, &devid, &padc);
    
    if(0 == rc) {
        return padc->driver_api->hadc_get_resolution(padc, devid);
    }
    return rc;
}

/* Returns the positive reference voltage for a maximum ADC reading.
 * This API assumes the negative reference voltage is zero volt.
 * Returns negative on error.  
 */
int 
hal_adc_get_reference_voltage_mvolts(int sysid)  
{
    int devid;
    struct hal_adc_s *padc;
    int rc = bsp_get_hal_adc_driver(sysid, &devid, &padc);
    
    if(0 == rc) {
        return padc->driver_api->hadc_get_reference_mvolts(padc, devid);
    }
    return rc;
}

/* returns the ADC read value converted to mvolts or negative on error */
int 
hal_adc_val_convert_to_mvolts(int sysid, int val) 
{
    int ret_val = -1;
    
    if(val >= 0)  {
        int ref;
        ref = hal_adc_get_reference_voltage_mvolts(sysid);   
        if(ref > 0) {
            val *= ref;
            ref = hal_adc_get_resolution(sysid);            
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