#if 0
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
#include <hal/hal_dac.h>
#include <hal/hal_dac_int.h>
#include <mcu/hal_dac.h>

/* forwards for the const structure below */
static int native_dac_write(struct hal_dac *pdac, int val);
static int native_dac_bits(struct hal_dac *pdac);
static int native_dac_current(struct hal_dac *pdac);
static int native_dac_refmv(struct hal_dac *pdac);

/* This DAC had four channels that print out the value  
 * random numbers */
const struct hal_dac_funcs native_dac_funcs = {
    .hdac_current = &native_dac_current,
    .hdac_get_ref_mv = &native_dac_refmv,
    .hdac_get_bits = &native_dac_bits,
    .hdac_write = &native_dac_write,
};

struct native_hal_dac {
    struct hal_dac parent;
    uint8_t        channel;
    uint8_t        last_written_val;
};

struct hal_dac * 
native_dac_create(enum native_dac_channel chan) 
{
    struct native_hal_dac *pdac = NULL;
    
    switch(chan) {
        case NATIVE_MCU_DAC0:
        case NATIVE_MCU_DAC1:
        case NATIVE_MCU_DAC2:
        case NATIVE_MCU_DAC3:
            pdac = malloc(sizeof(struct native_hal_dac));
            if (pdac) {
                pdac->parent.driver_api = &native_dac_funcs;
                pdac->channel = chan;
                pdac->last_written_val = 0;
            }
    }    
    return &pdac->parent;
}

int 
native_dac_bits(struct hal_dac *pdac) 
{
    struct native_hal_dac *pn = (struct native_hal_dac *) pdac;
    if (pn && pn->parent.driver_api == &native_dac_funcs) {
         return 8;        
    }
    return -1;
}

int 
native_dac_refmv(struct hal_dac *pdac) 
{
    struct native_hal_dac *pn = (struct native_hal_dac *) pdac;
    if (pn && pn->parent.driver_api == &native_dac_funcs) {
         return 5000;        
    }
    return -1;
}


int 
native_dac_current(struct hal_dac *pdac) 
{
    struct native_hal_dac *pn = (struct native_hal_dac *) pdac;
    if (pn && pn->parent.driver_api == &native_dac_funcs) {
         return (int) pn->last_written_val;        
    }
    return -1;
}

int 
native_dac_write(struct hal_dac *pdac, int val) 
{
    struct native_hal_dac *pn = (struct native_hal_dac *) pdac;
    
    if (val > 255) {
        val = 255;
    }
        
    if (pn && pn->parent.driver_api == &native_dac_funcs) {
        pn->last_written_val = val;
        return 0;
    }
    return -1;    
}
#endif
