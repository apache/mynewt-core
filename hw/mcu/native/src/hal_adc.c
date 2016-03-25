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
#include <mcu/mcu_devid.h>
#include <mcu/mcu_hal.h>

/* forwards for the const structure below */
static int native_adc_random_read(struct hal_adc_device_s *padc);
static int native_adc_random_get_resolution(struct hal_adc_device_s *padc);
static int native_adc_random_get_reference_mvolts(struct hal_adc_device_s *padc);

/* This ADC had two channels that return 8-bit 
 * random numbers */
const struct hal_adc_funcs_s random_adc_funcs = {
    .hadc_get_reference_mvolts = &native_adc_random_get_reference_mvolts,
    .hadc_get_resolution = &native_adc_random_get_resolution,
    .hadc_read = &native_adc_random_read,
};

struct hal_adc_device_s * 
native_adc_random_create(enum McuDeviceDescriptor devid) 
{
    struct hal_adc_device_s *padc = NULL;
    if((devid == MCU_ADC_CHANNEL_0) || (devid == MCU_ADC_CHANNEL_1)) {
        padc = malloc(sizeof(struct hal_adc_device_s));
        
        if(padc) {
            padc->driver_api = &random_adc_funcs;
            
            /* no other internal state to set */
        }
    }
    return padc;
}

static int 
native_adc_random_get_resolution(struct hal_adc_device_s *padc) 
{
    if(padc && padc->driver_api == &random_adc_funcs) {
         return 8;        
    }
    return -1;
}

static int 
native_adc_random_get_reference_mvolts(struct hal_adc_device_s *padc) 
{
    if(padc && padc->driver_api == &random_adc_funcs) {
        return 5000;    
    }
    return -1;
}

static int 
native_adc_random_read(struct hal_adc_device_s *padc) 
{
    if(padc && padc->driver_api == &random_adc_funcs) {
    return rand() & 0xff;
    }
    return -1;    
}

/* forwards for the const structure below */
static int native_adc_mmm_read(struct hal_adc_device_s *padc);
static int native_adc_mmm_get_resolution(struct hal_adc_device_s *padc);
static int native_adc_mmm_get_reference_mvolts(struct hal_adc_device_s *padc);

/* This ADC had two channels that return 8-bit 
 * random numbers */
const struct hal_adc_funcs_s mmm_adc_funcs = {
    .hadc_get_reference_mvolts = &native_adc_mmm_get_reference_mvolts,
    .hadc_get_resolution = &native_adc_mmm_get_resolution,
    .hadc_read = &native_adc_mmm_read,
};

/* This driver needs to keep a bit of state */
struct mmm_adc_device_s {
    struct hal_adc_device_s parent;
    enum McuDeviceDescriptor devid;   
};

struct hal_adc_device_s*
native_adc_mmm_create(enum McuDeviceDescriptor devid) 
{
    struct mmm_adc_device_s *padc = NULL;
    if(        (devid == MCU_ADC_CHANNEL_2) 
            || (devid == MCU_ADC_CHANNEL_3)
            || (devid == MCU_ADC_CHANNEL_4)) {
        padc = malloc(sizeof(struct mmm_adc_device_s));
        
        if(padc) {
            padc->parent.driver_api = &mmm_adc_funcs;
            padc->devid = devid;
        }
    }
    return &padc->parent;
}

static int 
native_adc_mmm_get_resolution(struct hal_adc_device_s *padc) 
{
    if(padc && padc->driver_api == &mmm_adc_funcs) {
         return 12;        
    }
    return -1;
}

static int 
native_adc_mmm_get_reference_mvolts(struct hal_adc_device_s *padc) 
{
    if(padc && padc->driver_api == &mmm_adc_funcs) {
        return 3300;    
    }
    return -1;
}

static int 
native_adc_mmm_read(struct hal_adc_device_s *padc) 
{
    struct mmm_adc_device_s *pmmmadc = (struct mmm_adc_device_s*) padc;
    
    if(padc && padc->driver_api == &mmm_adc_funcs) {

        switch(pmmmadc->devid) {
            case MCU_ADC_CHANNEL_2:
                return 0;
            case MCU_ADC_CHANNEL_3:
                return 0xfff >> 1;
            case MCU_ADC_CHANNEL_4:
                return 0xfff;
            default:
                break;
        }
    }
    
    return -1;    
}

/* forwards for the const structure below */
static int native_adc_file_read(struct hal_adc_device_s *padc);
static int native_adc_file_get_resolution(struct hal_adc_device_s *padc);
static int native_adc_file_get_reference_mvolts(struct hal_adc_device_s *padc);

/* This ADC had two channels that return 8-bit 
 * random numbers */
const struct hal_adc_funcs_s file_adc_funcs = {
    .hadc_get_reference_mvolts = &native_adc_file_get_reference_mvolts,
    .hadc_get_resolution = &native_adc_file_get_resolution,
    .hadc_read = &native_adc_file_read,
};

/* This driver needs to keep a bit of state */
struct file_adc_device_s {
    struct hal_adc_device_s  parent;
    FILE                    *native_fs;
};

struct hal_adc_device_s*
native_adc_file_create(enum McuDeviceDescriptor devid, char *fname) 
{
    struct file_adc_device_s *padc = NULL;
    if(devid == MCU_ADC_CHANNEL_5) {
        padc = malloc(sizeof(struct file_adc_device_s));
        
        if(padc) {
            padc->parent.driver_api = &file_adc_funcs;
            padc->native_fs = fopen(fname, "r");
            if(padc->native_fs <= 0) {
                free(padc);
                padc = NULL;
            }
        } 
    }
    return &padc->parent;
}

static int 
native_adc_file_get_resolution(struct hal_adc_device_s *padc) 
{
    if(padc && padc->driver_api == &file_adc_funcs) {
         return 8;        
    }
    return -1;
}

static int 
native_adc_file_get_reference_mvolts(struct hal_adc_device_s *padc) 
{
    if(padc && padc->driver_api == &file_adc_funcs) {
        return 3600;    
    }
    return -1;
}

static int 
native_adc_file_read(struct hal_adc_device_s *padc) 
{
    int val, rc;    
    struct file_adc_device_s *pfileadc = (struct file_adc_device_s*) padc;
    
    if(padc && (padc->driver_api == &file_adc_funcs) && (pfileadc->native_fs > 0)){
        
        rc = fread( &val, 1, 1, pfileadc->native_fs);

        if (rc == 1) {
            return (int) (val & 0xff);
        } else {
            fclose(pfileadc->native_fs);
        }      
    }
    
    return -1;    
}



#if 0


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
native_adc_file_init(struct hal_adc_device_s *padc) 
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
native_adc_file_get_resolution(struct hal_adc_device_s *padc) 
{
    DEVID_CHECK(MAX_FILE_ADC_DEVICES);
    return 8;
}

int 
native_adc_file_get_reference_mvolts(struct hal_adc_device_s *padc) 
{
    DEVID_CHECK(MAX_FILE_ADC_DEVICES);
    return 5000;    
}

int 
native_adc_file_read(struct hal_adc_device_s *padc) 
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

#endif