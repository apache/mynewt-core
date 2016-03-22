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

#ifndef HAL_PWM_INT_H
#define HAL_PWM_INT_H

#include <inttypes.h>

/* when you are implementing a driver for the hal_pwm. This is the interface
 * you must provide.  Devid is the number space defined by a single instance
 * of your driver.  For example if you have a port with 8 PWMs, than you
 * might have 8 devids (0-7) for your device.  
 */

struct hal_pwm_s;

struct hal_pwm_funcs_s {
    int (*hpwm_init)        (struct hal_pwm_s *inst, int devid);
    int (*hpwm_on)          (struct hal_pwm_s *inst, int devid);
    int (*hpwm_off)         (struct hal_pwm_s *inst, int devid);
    int (*hpwm_period_usec) (struct hal_pwm_s *inst, int devid,uint32_t period_usec);
    int (*hpwm_on_usec)     (struct hal_pwm_s *inst, int devid, uint32_t on_usec);
};

struct hal_pwm_s {
    const struct hal_pwm_funcs_s *driver_api;
};

extern int
bsp_get_hal_pwm_driver(int sysid,
                       int *devid_out,
                       struct hal_pwm_s **ppwm_out);

#endif /* HAL_PWM_INT_H */

