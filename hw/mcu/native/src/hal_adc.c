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
    
int native_adc_random_init(int devid) {
    DEVID_CHECK(2);
    /* nothing to do since this is simulated */
    return 0;
}

int native_adc_random_get_resolution(int devid) {
     DEVID_CHECK(2);
     return 8;
}

int native_adc_random_get_reference_mvolts(int devid) {
     DEVID_CHECK(2);
     return 5000;    
}

int native_adc_random_read(int devid) {
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

const struct hal_adc_s native_random_adc = {
    .driver_api = &random_adc_funcs,
};

int native_adc_mmm_init(int devid) {
    DEVID_CHECK(3);
    /* nothing to do since this is simulated */
    return 0;
}

int native_adc_mmm_get_resolution(int devid) {
     DEVID_CHECK(3);
     return 12;
}

int native_adc_mmm_get_reference_mvolts(int devid) {
     DEVID_CHECK(3);
     return 5000;    
}

int native_adc_mmm_read(int devid) {
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

const struct hal_adc_s native_min_mid_max_adc = {
    .driver_api = &mmm_adc_funcs,
};


FILE *native_fs;

int native_adc_file_init(int devid) {
    DEVID_CHECK(1);
    char fname[64];
    
    /* try to open a file with the name native_adc_%d.bin */
    sprintf(fname, "./native_adc_%d.bin", devid);
    native_fs = fopen(fname, "r");
    
    if(native_fs <= 0) {
        return -1;
    }
    
    return 0;
}

int native_adc_file_get_resolution(int devid) {
     DEVID_CHECK(1);
     return 8;
}

int native_adc_file_get_reference_mvolts(int devid) {
     DEVID_CHECK(1);
     return 5000;    
}

int native_adc_file_read(int devid) {
    int rc;
    uint8_t val; 
    DEVID_CHECK(1);
     
    rc = fread( &val, 1, 1, native_fs);
    
    if (rc == 1) {
        return (int) val;
    } else {
        fclose(native_fs);
    }
    
    return -1;
}

static const struct hal_adc_funcs_s file_adc_funcs = {
    .hadc_get_reference_mvolts = &native_adc_file_get_reference_mvolts,
    .hadc_get_resolution = &native_adc_file_get_resolution,
    .hadc_init = &native_adc_file_init,
    .hadc_read = &native_adc_file_read,
};

/* this ADC has one channel that reads bytes from 
 * a file and returns 8-bit ADC values .  When
 it reaches the end of the file, it repeats */
const struct hal_adc_s native_file_adc = {
    .driver_api = &file_adc_funcs,
};
