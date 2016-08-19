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
hal_pwm_disable(struct hal_pwm *ppwm) 
{
    if (ppwm && ppwm->driver_api && ppwm->driver_api->hpwm_disable ) 
    {
        return ppwm->driver_api->hpwm_disable(ppwm);
    }
    return -1;      
}

int
hal_pwm_enable_duty_cycle(struct hal_pwm *ppwm, uint16_t fraction) {
    if (ppwm && ppwm->driver_api && ppwm->driver_api->hpwm_ena_duty ) 
    {
        return ppwm->driver_api->hpwm_ena_duty(ppwm, fraction);
    }
    return -1;      
}

int
hal_pwm_set_frequency(struct hal_pwm *ppwm, uint32_t freq_hz) {
    if (ppwm && ppwm->driver_api && ppwm->driver_api->hpwm_set_freq) 
    {
        return ppwm->driver_api->hpwm_set_freq(ppwm, freq_hz);
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

int
hal_pwm_get_resolution_bits(struct hal_pwm *ppwm) {
    if (ppwm && ppwm->driver_api && ppwm->driver_api->hpwm_get_bits ) 
    {
        return ppwm->driver_api->hpwm_get_bits(ppwm);
    }
    return -1;        
}
