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

#ifndef _HAL_HAL_PWM_H
#define _HAL_HAL_PWM_H

#include <inttypes.h>
#include <bsp/bsp_sysid.h>

/* This is an abstract hardware API to Pulse Width Modulators.
 * A Pulse width module produces an output pulse stream with 
 * a specified period, and duty cycle. */
struct hal_pwm;

/* Initialize a new PWM device with the given system id.
 * Returns negative on error, 0 on success*/
struct hal_pwm*
hal_pwm_init(enum system_device_id sysid);

/* sets the period of the PWM *ppwm. The 
 * period is specified as micro_seconds.
 * Returns negative on error, 0 on success  
 */
int
hal_pwm_period_usec_set(struct hal_pwm *ppwm, uint32_t period_usec);

/* set the on duration of the PWM *ppwm.
 * The on duration must be less than or equal to the
 * period. Returns negative on error, 0 on success
 */
int
hal_pwm_on_usec_set(struct hal_pwm *ppwm, uint32_t on_usec);

/* enables the PWM corresponding to PWM *ppwm.*/
int
hal_pwm_on(struct hal_pwm *ppwm);

/* disables the PWM corresponding to PWM *ppwm.*/
int
hal_pwm_off(struct hal_pwm *ppwm);

#endif /* _HAL_HAL_PWM_H */

