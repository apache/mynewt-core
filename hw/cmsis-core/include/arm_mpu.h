/*
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

#ifndef ARM_MPU_H
#define ARM_MPU_H

#include <os/mynewt.h>
#include <os/link_tables.h>

#if __MPU_PRESENT
#if (__CORTEX_M == 0) || (__CORTEX_M == 3) || (__CORTEX_M == 4) || (__CORTEX_M == 7)

LINK_TABLE(ARM_MPU_Region_t, mpu_regions);

#define MPU_ARM_V7_REGION(name, address, rasr)                                \
    LINK_TABLE_ELEMENT(mpu_regions, name) = {                                 \
        .RBAR = address | MPU_RBAR_VALID_Msk,                                 \
        .RASR = rasr,                                                         \
    };

#elif (__CORTEX_M == 23) || (__CORTEX_M == 33) || (__CORTEX_M == 34) ||       \
    (__CORTEX_M == 35)
/* TODO: Add support for MPU ARMV8 */
#endif

void arm_mpu_init(void);

#endif
#endif
