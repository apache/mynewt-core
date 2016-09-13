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
#if 0
#include <fcntl.h>
#include <unistd.h>
#include <inttypes.h>
#include <stdlib.h>
#include <hal/hal_adc.h>
#include <hal/hal_adc_int.h>
#include <mcu/mcu_hal.h>


/* forwards for the const structure below */
static int native_adc_rand_read(struct hal_adc *padc);
static int native_adc_rand_bits(struct hal_adc *padc);
static int native_adc_rand_refmv(struct hal_adc *padc);

/* This ADC had two channels that return 8-bit 
 * random numbers */
const struct hal_adc_funcs random_adc_funcs = {
    .hadc_get_ref_mv = &native_adc_rand_refmv,
    .hadc_get_bits = &native_adc_rand_bits,
    .hadc_read = &native_adc_rand_read,
};

struct hal_adc * 
native_adc_random_create(enum native_adc_channel chan) 
{
    struct hal_adc *padc = NULL;
    if ((chan == MCU_ADC_CHANNEL_0) || (chan == MCU_ADC_CHANNEL_1)) {
        padc = malloc(sizeof(struct hal_adc));
        
        if (padc) {
            padc->driver_api = &random_adc_funcs;
            
            /* no other internal state to set */
        }
    }
    return padc;
}

static int 
native_adc_rand_bits(struct hal_adc *padc) 
{
    if (padc && padc->driver_api == &random_adc_funcs) {
         return 8;        
    }
    return -1;
}

static int 
native_adc_rand_refmv(struct hal_adc *padc) 
{
    if (padc && padc->driver_api == &random_adc_funcs) {
        return 5000;    
    }
    return -1;
}

static int 
native_adc_rand_read(struct hal_adc *padc) 
{
    if (padc && padc->driver_api == &random_adc_funcs) {
    return rand() & 0xff;
    }
    return -1;    
}

/* forwards for the const structure below */
static int native_adc_mmm_read(struct hal_adc *padc);
static int native_adc_mmm_get_bits(struct hal_adc *padc);
static int native_adc_mmm_get_refmv(struct hal_adc *padc);

/* This ADC had two channels that return 8-bit 
 * random numbers */
const struct hal_adc_funcs mmm_adc_funcs = {
    .hadc_get_ref_mv = &native_adc_mmm_get_refmv,
    .hadc_get_bits = &native_adc_mmm_get_bits,
    .hadc_read = &native_adc_mmm_read,
};

/* This driver needs to keep a bit of state */
struct mmm_adc_device {
    struct hal_adc parent;
    enum native_adc_channel  channel;   
};

struct hal_adc*
native_adc_mmm_create(enum native_adc_channel  chan) 
{
    struct mmm_adc_device *padc = NULL;
    if ((chan == MCU_ADC_CHANNEL_2) ||
        (chan == MCU_ADC_CHANNEL_3) ||
        (chan == MCU_ADC_CHANNEL_4)) {
        padc = malloc(sizeof(struct mmm_adc_device));
        
        if (padc) {
            padc->parent.driver_api = &mmm_adc_funcs;
            padc->channel = chan;
        }
    }
    return &padc->parent;
}

static int 
native_adc_mmm_get_bits(struct hal_adc *padc) 
{
    if (padc && padc->driver_api == &mmm_adc_funcs) {
         return 12;        
    }
    return -1;
}

static int 
native_adc_mmm_get_refmv(struct hal_adc *padc) 
{
    if (padc && padc->driver_api == &mmm_adc_funcs) {
        return 3300;    
    }
    return -1;
}

static int 
native_adc_mmm_read(struct hal_adc *padc) 
{
    struct mmm_adc_device *pmmmadc = (struct mmm_adc_device*) padc;
    
    if (padc && padc->driver_api == &mmm_adc_funcs) {

        switch(pmmmadc->channel) {
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
static int native_adc_file_read(struct hal_adc *padc);
static int native_adc_file_get_bits(struct hal_adc *padc);
static int native_adc_file_get_refmv(struct hal_adc *padc);

/* This ADC had two channels that return 8-bit 
 * random numbers */
const struct hal_adc_funcs file_adc_funcs = {
    .hadc_get_ref_mv = &native_adc_file_get_refmv,
    .hadc_get_bits = &native_adc_file_get_bits,
    .hadc_read = &native_adc_file_read,
};

/* This driver needs to keep a bit of state */
struct file_adc_device {
    struct hal_adc  parent;
    int             native_fs;
};

struct hal_adc*
native_adc_file_create(enum native_adc_channel chan, const char *fname) 
{
    struct file_adc_device *padc = NULL;
    if (chan == MCU_ADC_CHANNEL_5) {
        padc = malloc(sizeof(struct file_adc_device));
        
        if (padc) {
            padc->parent.driver_api = &file_adc_funcs;
            padc->native_fs = open(fname, O_RDONLY);
            if (padc->native_fs < 0) {
                free(padc);
                padc = NULL;
            }
        } 
    }
    return &padc->parent;
}

static int 
native_adc_file_get_bits(struct hal_adc *padc) 
{
    if (padc && padc->driver_api == &file_adc_funcs) {
         return 8;        
    }
    return -1;
}

static int 
native_adc_file_get_refmv(struct hal_adc *padc) 
{
    if (padc && padc->driver_api == &file_adc_funcs) {
        return 3600;    
    }
    return -1;
}

static int 
native_adc_file_read(struct hal_adc *padc) 
{
    uint8_t val;
    int rc;    
    struct file_adc_device *pfileadc = (struct file_adc_device*) padc;
    
    if ( padc && (padc->driver_api == &file_adc_funcs) && 
         (pfileadc->native_fs > 0) ){
        
        rc = read(pfileadc->native_fs,  &val, 1);

        if (rc == 1) {
            return (int) val;
        } else {
            close(pfileadc->native_fs);
        }      
    }
    
    return -1;    
}

#endif
