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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hal/hal_adc.h>
#include <mcu/native_bsp.h>
#include <bsp/bsp.h>

/* initialize the ADC corresponding to adc device padc.
 * Returns 0 on success.  the ADC must be initialized
 * before calling any other functions in this header file .
 * If other methods are called with a sysid before init is 
 * called, the function will return error */
int hal_adc_init(struct hal_adc_device_s *padc, enum BspPinDescriptor pin) {
    
    memset(padc, 0, sizeof(*padc));
    
    switch(pin) {
        case MCU_PIN_0:
        case MCU_PIN_1:
            padc->type = ADC_RANDOM;
            break;
        case MCU_PIN_2:
            padc->type = ADC_MIN;
            break;
        case MCU_PIN_3:
            padc->type = ADC_MID;
            break;
        case MCU_PIN_4:
            padc->type = ADC_MAX;
            break;
        case MCU_PIN_5:
            padc->type = ADC_FILE;
            break;
        default:
            return -1;
    }
    
    switch(padc->type) {
        case ADC_FILE:
        {
            char fname[64];

            /* try to open a file with the name native_adc_%d.bin */
            sprintf(fname, "./native_adc_0.bin");
            padc->native_fs = fopen(fname, "r");

            if(padc->native_fs <= 0) {
                return -2;
            }        
            break;
        }
        default:
            /* nothing to do */
            break;
    }

    return 0;
}


/*
 * read the ADC corresponding to sysid in your system.  Returns
 * the  adc value read or negative on error, See 
 * hal_adc_get_resolution to check the range of the return value */
int hal_adc_read(struct hal_adc_device_s *padc) {
    int rc;
    int val;

    switch(padc->type) {
        case ADC_RANDOM:
            return (rand() & 0xff);
        case ADC_MIN:
            return 0;            
        case ADC_MID:
            return 0xff >> 1;
        case ADC_MAX:
            return 0xff;
        case ADC_FILE:
            
            if(padc->native_fs) {
                rc = fread( &val, 1, 1, padc->native_fs);

                if (rc == 1) {
                    return (int) (val & 0xff);
                } else {
                    fclose(padc->native_fs);
                    padc->native_fs = 0;
                }
                return rc;
            }
            return -2;
    }
    return -1;
}
        

/* returns the number of bit of resolution in this ADC.  
 * For example if the system has an 8-bit ADC reporting 
 * values from 0= to 255 (2^8-1), this function would return
 * the value 8. returns negative or zero on error */
int hal_adc_get_resolution(struct hal_adc_device_s *padc) {
    return 8;
}

/* Returns the positive reference voltage for a maximum ADC reading.
 * This API assumes the negative reference voltage is zero volt.
 * Returns negative or zero on error.  
 */
int hal_adc_get_reference_voltage_mvolts(struct hal_adc_device_s *padc) {
    return 5000;
}

/* Converts and ADC value to millivolts. This is a helper function
 * but does call the ADC to get the reference voltage and 
 * resolution */
int hal_adc_val_convert_to_mvolts(struct hal_adc_device_s *padc, int val)
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
                val += (1 << (ref - 1)) - 1;
                ret_val = (val >> ref);                
            }  else {
                ret_val = val;
            }
        }
    }
    return ret_val;
}  