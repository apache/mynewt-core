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
#include <hal/hal_pwm_int.h>
#include <mcu/native_bsp.h>
#include <console/console.h>

struct native_pwm_drv_s {
    struct hal_pwm_s driver;        // Must be first      
    uint32_t         on_usec[NUMDEVICE_PWMS];
    uint32_t         period_usec[NUMDEVICE_PWMS];
    int              status[NUMDEVICE_PWMS];
};

#define CHECK_DEVID       do {                                   \
                            if(devid >= NUMDEVICE_PWMS) {        \
                               return -1;                        \
                            }                                    \
                          } while(0);


int 
native_pwm_init (struct hal_pwm_s *pinst, int devid) 
{
    struct native_pwm_drv_s *pdrv = (struct native_pwm_drv_s*) pinst;
    
    CHECK_DEVID;
    
    pdrv->period_usec[devid] = 1000000;
    pdrv->on_usec[devid] = 500000;  
    pdrv->status[devid] = 0;
    return 0;
}

int 
native_pwm_on(struct hal_pwm_s *pinst, int devid) 
{
    struct native_pwm_drv_s *pdrv = (struct native_pwm_drv_s*) pinst;
    
    CHECK_DEVID;
    pdrv->status[devid] = 1;
    console_printf("\nDevice %p %d started with period=%u on=%u\n", 
            pinst, devid, pdrv->period_usec[devid], pdrv->on_usec[devid]);
    return 0;
}

int 
native_pwm_off(struct hal_pwm_s *pinst, int devid)
{
    struct native_pwm_drv_s *pdrv = (struct native_pwm_drv_s*) pinst;
    
    CHECK_DEVID; 
    
    if(pdrv->status[devid]) {
        pdrv->status[devid] = 0;    
        console_printf("Device %p %d stopped with period=%u on=%u\n", 
                pinst, devid, pdrv->period_usec[devid], pdrv->on_usec[devid]);    
    }
    return 0;
}

int 
native_pwm_period_usec(struct hal_pwm_s *pinst, int devid, uint32_t period_usec)
{
    struct native_pwm_drv_s *pdrv = (struct native_pwm_drv_s*) pinst;
    CHECK_DEVID; 
    
    if(period_usec < pdrv->on_usec[devid]) {
        return -1;
    }
    
    pdrv->period_usec[devid] = period_usec;
    return 0;
}

int native_pwm_on_usec(struct hal_pwm_s *pinst, int devid, uint32_t on_usec) 
{
    struct native_pwm_drv_s *pdrv = (struct native_pwm_drv_s*) pinst;
    CHECK_DEVID;
    
    if(on_usec > pdrv->period_usec[devid]) {
        return -1;
    }
    
    pdrv->on_usec[devid] = on_usec;    
    return 0;
}

/* the function API for the driver  */
static const struct hal_pwm_funcs_s  native_pwm_funcs = 
{
    .hpwm_on_usec = &native_pwm_on_usec,
    .hpwm_init = &native_pwm_init,
    .hpwm_off = &native_pwm_off,
    .hpwm_on = &native_pwm_on,
    .hpwm_period_usec = &native_pwm_period_usec,
};

/* The internal driver structure */
static struct native_pwm_drv_s gstate = 
{
    .driver.driver_api = &native_pwm_funcs,
};

/* External reference for the driver to give outside the MCU */
struct hal_pwm_s * pnative_pwm_dev = &gstate.driver;
