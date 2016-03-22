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

#include <hal/hal_pwm.h>
#include <hal/hal_pwm_int.h>

int
hal_pwm_init(int sysid) 
{
    int devid;
    struct hal_pwm_s *ppwm;
    int rc = bsp_get_hal_pwm_driver(sysid, &devid, &ppwm);
    
    if(0 == rc) {
        return ppwm->driver_api->hpwm_init(ppwm, devid);
    }
    return rc;    
}

int
hal_pwm_period_usec_set(int sysid, uint32_t period_usec) 
{
    int devid;
    struct hal_pwm_s *ppwm;
    int rc = bsp_get_hal_pwm_driver(sysid, &devid, &ppwm);
    
    if(0 == rc) {
        return ppwm->driver_api->hpwm_period_usec(ppwm, devid, period_usec);
    }
    return rc;    
}

int
hal_pwm_on_usec_set(int sysid, uint32_t on_usec)
{
    int devid;
    struct hal_pwm_s *ppwm;
    int rc = bsp_get_hal_pwm_driver(sysid, &devid, &ppwm);
    
    if(0 == rc) {
        return ppwm->driver_api->hpwm_on_usec(ppwm, devid, on_usec);
    }  
    return rc;    
}

int
hal_pwm_on(int sysid) 
{
    int devid;
    struct hal_pwm_s *ppwm;
    int rc = bsp_get_hal_pwm_driver(sysid, &devid, &ppwm);
    
    if(0 == rc) {
        return ppwm->driver_api->hpwm_on(ppwm, devid);
    } 
    return rc;
}

int
hal_pwm_off(int sysid) 
{
    int devid;
    struct hal_pwm_s *ppwm;
    int rc = bsp_get_hal_pwm_driver(sysid, &devid, &ppwm);
    
    if(0 == rc) {
        return ppwm->driver_api->hpwm_off(ppwm, devid);
    }  
    return rc;
}
