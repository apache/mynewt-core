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

/* enables the PWM corresponding to PWM *ppwm.*/
int
hal_pwm_on(struct hal_pwm *ppwm);

/* disables the PWM corresponding to PWM *ppwm.*/
int
hal_pwm_off(struct hal_pwm *ppwm);

/* There are two APIs for setting the PWM waveform.  You can use one 
 *  or both in your programs, but only the last one called will apply
 *
 * hal_pwm_set_duty_cycle -- sets the PWM waveform for a specific duty cycle
 *          It uses a 255 clock period and sets the on and off time 
 *          according to the argument
 * 
 * hal_pwm_set_waveform -- sets the PWM waveform for a specific on time
 *          and period specified in PWM clocks.  its intended for more
 *          fine-grained control of the PWM waveform.  
 *
 */

/* sets the duty cycle of the PWM output. This duty cycle is 
 * a fractional duty cycle where 0 == off, 255 = on, and 
 * any value in between is on for fraction clocks and off
 * for 255-fraction clocks. 
 * When you are looking for more fine-grained control over
 * the PWM, use the API set_waveform below.
 */
int
hal_pwm_set_duty_cycle(struct hal_pwm *ppwm, uint8_t fraction);


/* Sets the pwm waveform period and on-time in units of the PWM clock 
  (see below).  Period_clocks and on_clocks cannot exceed the 
 * 2^N-1 where N is the resolution of the PWM channel  */
int
hal_pwm_set_waveform(struct hal_pwm *ppwm, uint32_t period_clocks, uint32_t on_clocks);


/* gets the underlying clock driving the PWM output. Return value
 * is in Hz. Returns negative on error */
int 
hal_pwm_get_clock_freq(struct hal_pwm *ppwm);

/* gets the resolution of the PWM in bits.  An N-bit PWM can have 
 * period and on values between 0 and 2^bits - 1. Returns negative on error  */
int
hal_pwm_get_resolution_bits(struct hal_pwm *ppwm);


#endif /* _HAL_HAL_PWM_H */

