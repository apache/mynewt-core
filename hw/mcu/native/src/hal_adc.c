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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <hal/hal_adc.h>
#include <hal/hal_adc_int.h>
#include <mcu/native_bsp.h>

#define DEVID_CHECK(max) do {                                           \
                            if(padc->device_id > max) {                 \
                               return -1;                               \
                            }                                           \
                        } while(0);           
    
int 
native_adc_random_init(const struct hal_adc_device_s *padc) 
{
    DEVID_CHECK(MAX_RANDOM_ADC_DEVICES);
    /* nothing to do since this is simulated */
    return 0;
}

int 
native_adc_random_get_resolution(const struct hal_adc_device_s *padc) 
{
     DEVID_CHECK(MAX_RANDOM_ADC_DEVICES);
     return 8;
}

int 
native_adc_random_get_reference_mvolts(const struct hal_adc_device_s *padc) 
{
     DEVID_CHECK(MAX_RANDOM_ADC_DEVICES);
     return 5000;    
}

int 
native_adc_random_read(const struct hal_adc_device_s *padc) 
{
     DEVID_CHECK(MAX_RANDOM_ADC_DEVICES);
    return rand() & 0xff;
}

/* This ADC had two channels that return 8-bit 
 * random numbers */
const struct hal_adc_funcs_s random_adc_funcs = {
    .hadc_get_reference_mvolts = &native_adc_random_get_reference_mvolts,
    .hadc_get_resolution = &native_adc_random_get_resolution,
    .hadc_init = &native_adc_random_init,
    .hadc_read = &native_adc_random_read,
};

int 
native_adc_mmm_init(const struct hal_adc_device_s *padc) 
{
    DEVID_CHECK(MAX_MMM_ADC_DEVICES);
    /* nothing to do since this is simulated */
    return 0;
}

int 
native_adc_mmm_get_resolution(const struct hal_adc_device_s *padc) 
{
     DEVID_CHECK(MAX_MMM_ADC_DEVICES);
     return 12;
}

int 
native_adc_mmm_get_reference_mvolts(const struct hal_adc_device_s *padc) 
{
     DEVID_CHECK(MAX_MMM_ADC_DEVICES);
     return 5000;    
}

int 
native_adc_mmm_read(const struct hal_adc_device_s *padc) 
{
     switch(padc->device_id) {
         case 0:
             return 0;
         case 1:
             return 2047;
         case 2:
             return 4095;
         default:
             return -1;
     }
}

const struct hal_adc_funcs_s mmm_adc_funcs = {
    .hadc_get_reference_mvolts = &native_adc_mmm_get_reference_mvolts,
    .hadc_get_resolution = &native_adc_mmm_get_resolution,
    .hadc_init = &native_adc_mmm_init,
    .hadc_read = &native_adc_mmm_read,
};

/* This driver reads ADC values from a file . It stores some internal 
 * state for itself. It does this by  wrapping the driver structure 
 * with its own state and then casting back to its own driver pointer
 * inside the methods */
struct adc_fldrv_state_s {
    FILE *native_fs;
};

/* here I decided to allocate a maximum number of these.  To be
 * memory conscious, I could malloc these and link them together */
struct adc_fldrv_state_s g_state[MAX_FILE_ADC_DEVICES];    

int 
native_adc_file_init(const struct hal_adc_device_s *padc) 
{
    struct adc_fldrv_state_s *pstate = &g_state[padc->device_id];
    char fname[64];
    DEVID_CHECK(MAX_FILE_ADC_DEVICES);
    
    /* try to open a file with the name native_adc_%d.bin */
    sprintf(fname, "./native_adc_%d.bin", padc->device_id);
    pstate->native_fs = fopen(fname, "r");
    
    if(pstate->native_fs <= 0) {
        return -1;
    }
    
    return 0;
}

int 
native_adc_file_get_resolution(const struct hal_adc_device_s *padc) 
{
    DEVID_CHECK(MAX_FILE_ADC_DEVICES);
    return 8;
}

int 
native_adc_file_get_reference_mvolts(const struct hal_adc_device_s *padc) 
{
    DEVID_CHECK(MAX_FILE_ADC_DEVICES);
    return 5000;    
}

int 
native_adc_file_read(const struct hal_adc_device_s *padc) 
{
    struct adc_fldrv_state_s *pstate = &g_state[padc->device_id];
    int rc;
    uint8_t val; 
    DEVID_CHECK(MAX_FILE_ADC_DEVICES);
    
    if(pstate->native_fs <= 0) {
        return -2;
    }
     
    rc = fread( &val, 1, 1, pstate->native_fs);
    
    if (rc == 1) {
        return (int) val;
    } else {
        fclose(pstate->native_fs);
    }
    
    return -1;
}

const struct hal_adc_funcs_s file_adc_funcs = {
    .hadc_get_reference_mvolts = &native_adc_file_get_reference_mvolts,
    .hadc_get_resolution = &native_adc_file_get_resolution,
    .hadc_init = &native_adc_file_init,
    .hadc_read = &native_adc_file_read,
};
