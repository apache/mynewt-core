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
#include <bsp/bsp_sysid.h>
#include <hal/hal_pwm.h>
#include <hal/hal_pwm_int.h>

struct hal_pwm *
hal_pwm_init(enum system_device_id sysid) 
{
    return bsp_get_hal_pwm_driver(sysid);
}

int
hal_pwm_on(struct hal_pwm *ppwm) 
{
    if (ppwm && ppwm->driver_api && ppwm->driver_api->hpwm_on ) 
    {
        return ppwm->driver_api->hpwm_on(ppwm);
    }
    return -1;  
}

int
hal_pwm_off(struct hal_pwm *ppwm) 
{
    if (ppwm && ppwm->driver_api && ppwm->driver_api->hpwm_off ) 
    {
        return ppwm->driver_api->hpwm_off(ppwm);
    }
    return -1;      
}

int
hal_pwm_set_duty_cycle(struct hal_pwm *ppwm, uint8_t fraction) {
    if (ppwm && ppwm->driver_api && ppwm->driver_api->hpwm_set_duty ) 
    {
        return ppwm->driver_api->hpwm_set_duty(ppwm, fraction);
    }
    return -1;      
}

int
hal_pwm_set_waveform(struct hal_pwm *ppwm, uint32_t period_clocks, uint32_t on_clocks) {
    if (ppwm && ppwm->driver_api && ppwm->driver_api->hpwm_set_wave ) 
    {
        return ppwm->driver_api->hpwm_set_wave(ppwm, period_clocks, on_clocks);
    }
    return -1;       
}

int 
hal_pwm_get_clock_freq(struct hal_pwm *ppwm) {
    if (ppwm && ppwm->driver_api && ppwm->driver_api->hpwm_get_clk ) 
    {
        return ppwm->driver_api->hpwm_get_clk(ppwm);
    }
    return -1;     
}

/* gets the resolution of the PWM in bits.  An N-bit PWM can have 
 * period and on values between 0 and 2^bits - 1 */
int
hal_pwm_get_resolution_bits(struct hal_pwm *ppwm) {
    if (ppwm && ppwm->driver_api && ppwm->driver_api->hpwm_get_bits ) 
    {
        return ppwm->driver_api->hpwm_get_bits(ppwm);
    }
    return -1;        
}