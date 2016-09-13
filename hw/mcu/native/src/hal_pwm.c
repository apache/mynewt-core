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

#include <inttypes.h>
#include <stdlib.h>
#include <hal/hal_pwm.h>
#include <hal/hal_pwm_int.h>
#include <mcu/hal_pwm.h>
#include <console/console.h>

/* if you change this, you have to change sizes below */
#define NATIVE_PWM_BITS (16)
#define NATIVE_PWM_CLK_FREQ_HZ  (1000000)

struct native_pwm_drv {
    struct hal_pwm   driver;    
    uint8_t          channel;
    uint8_t          status;
    uint16_t         on_ticks;
    uint16_t         period_ticks;
};

static int native_pwm_off(struct hal_pwm *ppwm);
static int native_pwm_enable_duty(struct hal_pwm *ppwm, uint16_t frac_duty);
static int native_pwm_get_bits(struct hal_pwm *ppwm);
static int native_pwm_get_clock(struct hal_pwm *ppwm);

/* the function API for the driver  */
static const struct hal_pwm_funcs  native_pwm_funcs = 
{
    .hpwm_get_bits = &native_pwm_get_bits,
    .hpwm_get_clk = &native_pwm_get_clock,
    .hpwm_disable = &native_pwm_off,
    .hpwm_ena_duty = &native_pwm_enable_duty,
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
        ppwm->period_ticks = 0xffff;        
        /* 50% duty cycle */
        ppwm->on_ticks = ppwm->period_ticks >> 1;
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
                pn, pn->channel, pn->period_ticks, pn->on_ticks);
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
                pn, pn->channel, pn->period_ticks, pn->on_ticks);    
        return 0;
    }

    return -1;
}

static int native_pwm_get_bits(struct hal_pwm *ppwm) {
    if(ppwm) {
        return NATIVE_PWM_BITS;
    }
    return -1;
}

static int native_pwm_get_clock(struct hal_pwm *ppwm) {
    if(ppwm) {
        return NATIVE_PWM_CLK_FREQ_HZ;
    }
    return -1;
    
}

int 
native_pwm_enable_duty(struct hal_pwm *ppwm, uint16_t fraction)
{
    struct native_pwm_drv *pn = (struct native_pwm_drv *) ppwm;
 
    if(pn) 
    {    
        pn->period_ticks = 65535;
        pn->on_ticks = fraction;
        return 0;
    }
    return -2;
}
#endif
