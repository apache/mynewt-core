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

#ifndef _NATIVE_HAL_DAC_H
#define _NATIVE_HAL_DAC_H

#ifdef __cplusplus
extern "C" {
#endif

enum native_dac_channel 
{
    NATIVE_MCU_DAC0 = 0,
    NATIVE_MCU_DAC1,
    NATIVE_MCU_DAC2,
    NATIVE_MCU_DAC3,
};
 
/* to create a native dac driver */
struct hal_dac *
native_dac_create (enum native_dac_channel);

#ifdef __cplusplus
}
#endif

#endif /* _NATIVE_HAL_PWM_H */

