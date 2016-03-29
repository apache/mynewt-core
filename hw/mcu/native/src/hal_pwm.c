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
#include <stdlib.h>
#include <hal/hal_pwm.h>
#include <hal/hal_pwm_int.h>
#include <mcu/hal_pwm.h>
#include <console/console.h>

struct native_pwm_drv {
    struct hal_pwm   driver;    
    uint32_t         on_usec;
    uint32_t         period_usec;
    uint16_t         channel;
    uint16_t         status;
};

static int native_pwm_off(struct hal_pwm *ppwm);
static int native_pwm_on(struct hal_pwm *ppwm);
static int native_pwm_period_usec(struct hal_pwm *ppwm, uint32_t period_usec);
static int native_pwm_on_usec(struct hal_pwm *ppwm, uint32_t on_usec);

/* the function API for the driver  */
static const struct hal_pwm_funcs  native_pwm_funcs = 
{
    .hpwm_on_usec = &native_pwm_on_usec,
    .hpwm_off = &native_pwm_off,
    .hpwm_on = &native_pwm_on,
    .hpwm_period_usec = &native_pwm_period_usec,
};

struct hal_pwm *
native_pwm_create (enum native_pwm_channel chan)
{
    struct native_pwm_drv *ppwm;
    
    if(chan >= NATIVE_MCU_PWM_MAX) 
    {
        return NULL;
    }
    
    ppwm = malloc(sizeof(struct native_pwm_drv));
    
    if(ppwm) 
    {
        ppwm->driver.driver_api =  &native_pwm_funcs;
        ppwm->period_usec = 0xffffffff;
        ppwm->on_usec = 0;  
        ppwm->status = 0;
        ppwm->channel = chan;
    }
    return &ppwm->driver;
}

int 
native_pwm_on(struct hal_pwm *ppwm) 
{
    struct native_pwm_drv *pn = (struct native_pwm_drv *) ppwm;
    if(pn) 
    {
        console_printf("Device %p %d started with period=%u on=%u\n", 
                pn, pn->channel, pn->period_usec, pn->on_usec);
        return 0;
    }
    return -1;
}

int 
native_pwm_off(struct hal_pwm *ppwm)
{
    struct native_pwm_drv *pn = (struct native_pwm_drv *) ppwm;
 
    if(pn) 
    {
        console_printf("Device %p %d stopped with period=%u on=%u\n", 
                pn, pn->channel, pn->period_usec, pn->on_usec);    
        return 0;
    }

    return -1;
}

int 
native_pwm_period_usec(struct hal_pwm *ppwm, uint32_t period_usec)
{
    struct native_pwm_drv *pn = (struct native_pwm_drv *) ppwm;
 
    if(pn) 
    {    
        if(period_usec < pn->on_usec) 
        {
            return -1;
        }
        pn->period_usec = period_usec;
        return 0;
    }
    return -2;
}

int 
native_pwm_on_usec(struct hal_pwm *ppwm, uint32_t on_usec) 
{
    struct native_pwm_drv *pn = (struct native_pwm_drv *) ppwm;
 
    if(pn) 
    {        
        if(on_usec > pn->period_usec) 
        {
            return -1;
        }
    
        pn->on_usec = on_usec;    
        return 0;
    }
    return -2;
}

