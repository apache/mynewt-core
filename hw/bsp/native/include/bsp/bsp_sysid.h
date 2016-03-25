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
#ifndef __BSP_SYSID_H
#define __BSP_SYSID_H

/* This file defines the system device descriptors for this BSP.
 * System device descriptors are any HAL devies */

#ifdef __cplusplus
extern "C" {
#endif

/* this is a native build and these are just simulated */
enum SystemDeviceDescriptor {
    NATIVE_A0,
    NATIVE_A1,
    NATIVE_A2,
    NATIVE_A3,
    NATIVE_A4,
    NATIVE_A5,
    /* TODO -- add other hals here */
};

#ifdef __cplusplus
}
#endif

#endif  /* __BSP_SYSID_H */
