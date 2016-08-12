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
#ifndef __NATIVE_BSP_SYSID_H
#define __NATIVE_BSP_SYSID_H

/* This file defines the system device descriptors for this BSP.
 * System device descriptors are any HAL devies */

/* this is a native build and these are just simulated */
enum system_device_id {
    NATIVE_A0,
    NATIVE_A1,
    NATIVE_A2,
    NATIVE_A3,
    NATIVE_A4,
    NATIVE_A5,
    NATIVE_BSP_PWM_0,
    NATIVE_BSP_PWM_1,
    NATIVE_BSP_PWM_2,
    NATIVE_BSP_PWM_3,
    NATIVE_BSP_PWM_4,
    NATIVE_BSP_PWM_5,
    NATIVE_BSP_PWM_6,
    NATIVE_BSP_PWM_7, 
    NATIVE_BSP_DAC_0,
    NATIVE_BSP_DAC_1,
    NATIVE_BSP_DAC_2,
    NATIVE_BSP_DAC_3,
    /* TODO -- add other hals here */    
};

#endif  /* __NATIVE_BSP_SYSID_H */
