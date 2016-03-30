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

#ifndef _NATIVE_HAL_PWM_H
#define _NATIVE_HAL_PWM_H

enum native_pwm_channel 
{
    NATIVE_MCU_PWM0 = 0,
    NATIVE_MCU_PWM1,
    NATIVE_MCU_PWM2,
    NATIVE_MCU_PWM3,
    NATIVE_MCU_PWM4,
    NATIVE_MCU_PWM5,
    NATIVE_MCU_PWM6,
    NATIVE_MCU_PWM7,
    NATIVE_MCU_PWM_MAX
};
 
/* to create a pwm driver */
struct hal_pwm *
native_pwm_create (enum native_pwm_channel);

#ifdef __cplusplus
extern "C" {
#endif




#ifdef __cplusplus
}
#endif

#endif /* _NATIVE_HAL_PWM_H */

