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

#ifndef HAL_PWM_H
#define HAL_PWM_H

#include <inttypes.h>

/* This is an abstract hardware API to Pulse Width Modulators.
 * A Pulse width module produces an output pulse stream with 
 * a specified period, and duty cycle. */

/* Initialize a new PWM device with the given system id.
 * Returns -1 on error, 0 on success*/
int
hal_pwm_init(int sysid);

/* sets the period of the PWM corresponding to sysid. The 
 * period is specified as micro_seconds.
 * Returns -1 on error, 0 on success  
 */
int
hal_pwm_period_usec_set(int sysid, uint32_t period_usec);

/* set the on duration of the PWM corresponding to sysid.
 * The on duration must be less than or equal to the
 * period.  * Returns -1 on error, 0 on success
 */
int
hal_pwm_on_usec_set(int sysid, uint32_t on_usec);

/* enables the PWM corresponding to sysid.*/
int
hal_pwm_on(int sysid);

/* disables the PWM corresponding to sysid.*/
int
hal_pwm_off(int sysid);


#endif /* HAL_PWM_H */

