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

#define DEVID_CHECK(max) do {                                           \
                            if((devid < 0) || (devid > max)) {          \
                               return -1;                               \
                            }                                           \
                        } while(0);           
    
int 
native_adc_random_init(struct hal_adc_s *padc, int devid) 
{
    DEVID_CHECK(2);
    /* nothing to do since this is simulated */
    return 0;
}

int 
native_adc_random_get_resolution(struct hal_adc_s *padc, int devid) 
{
     DEVID_CHECK(2);
     return 8;
}

int 
native_adc_random_get_reference_mvolts(struct hal_adc_s *padc, int devid) 
{
     DEVID_CHECK(2);
     return 5000;    
}

int 
native_adc_random_read(struct hal_adc_s *padc, int devid) 
{
     DEVID_CHECK(2);
    return rand() & 0xff;
}

/* This ADC had two channels that return 8-bit 
 * random numbers */
static const struct hal_adc_funcs_s random_adc_funcs = {
    .hadc_get_reference_mvolts = &native_adc_random_get_reference_mvolts,
    .hadc_get_resolution = &native_adc_random_get_resolution,
    .hadc_init = &native_adc_random_init,
    .hadc_read = &native_adc_random_read,
};

static const struct hal_adc_s native_random_adc = {
    .driver_api = &random_adc_funcs,
};

const struct hal_adc_s *pnative_random_adc = &native_random_adc;

int 
native_adc_mmm_init(struct hal_adc_s *padc, int devid) 
{
    DEVID_CHECK(3);
    /* nothing to do since this is simulated */
    return 0;
}

int 
native_adc_mmm_get_resolution(struct hal_adc_s *padc, int devid) 
{
     DEVID_CHECK(3);
     return 12;
}

int 
native_adc_mmm_get_reference_mvolts(struct hal_adc_s *padc, int devid) 
{
     DEVID_CHECK(3);
     return 5000;    
}

int 
native_adc_mmm_read(struct hal_adc_s *padc, int devid) 
{
     switch(devid) {
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

static const struct hal_adc_funcs_s mmm_adc_funcs = {
    .hadc_get_reference_mvolts = &native_adc_mmm_get_reference_mvolts,
    .hadc_get_resolution = &native_adc_mmm_get_resolution,
    .hadc_init = &native_adc_mmm_init,
    .hadc_read = &native_adc_mmm_read,
};

static const struct hal_adc_s native_min_mid_max_adc = {
    .driver_api = &mmm_adc_funcs,
};

const struct hal_adc_s *pnative_min_mid_max_adc = &native_min_mid_max_adc;



/* This driver reads ADC values from a file . It stores some internal 
 * state for itself. It does this by  wrapping the driver structure 
 * with its own state and then casting back to its own driver pointer
 * inside the methods */
struct adc_fldrv_state_s {
    struct hal_adc_s driver;  // must be first 
    FILE *native_fs;    
};

int 
native_adc_file_init(struct hal_adc_s *padc, int devid) 
{
    struct adc_fldrv_state_s *pdrv = (struct adc_fldrv_state_s *) padc;    
    char fname[64];
    DEVID_CHECK(1);
    
    /* try to open a file with the name native_adc_%d.bin */
    sprintf(fname, "./native_adc_%d.bin", devid);
    pdrv->native_fs = fopen(fname, "r");
    
    if(pdrv->native_fs <= 0) {
        return -1;
    }
    
    return 0;
}

int 
native_adc_file_get_resolution(struct hal_adc_s *padc, int devid) 
{
    DEVID_CHECK(1);
    return 8;
}

int 
native_adc_file_get_reference_mvolts(struct hal_adc_s *padc, int devid) 
{
    DEVID_CHECK(1);
    return 5000;    
}

int 
native_adc_file_read(struct hal_adc_s *padc, int devid) 
{
    struct adc_fldrv_state_s *pdrv = (struct adc_fldrv_state_s *) padc;
    int rc;
    uint8_t val; 
    DEVID_CHECK(1);
    
    if(pdrv->native_fs <= 0) {
        return -2;
    }
     
    rc = fread( &val, 1, 1, pdrv->native_fs);
    
    if (rc == 1) {
        return (int) val;
    } else {
        fclose(pdrv->native_fs);
    }
    
    return -1;
}

static const struct hal_adc_funcs_s file_adc_funcs = {
    .hadc_get_reference_mvolts = &native_adc_file_get_reference_mvolts,
    .hadc_get_resolution = &native_adc_file_get_resolution,
    .hadc_init = &native_adc_file_init,
    .hadc_read = &native_adc_file_read,
};

static struct adc_fldrv_state_s internal_state = {
    .driver.driver_api = &file_adc_funcs,
};

/* external pointer to the driver interface */
const struct hal_adc_s *pnative_file_adc = &internal_state.driver;
