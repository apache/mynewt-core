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

#include <os/os.h>
#include <os/link_tables.h>
#include <arm_mpu.h>

#ifndef __CORTEX_M
#error "Macro __CORTEX_M not defined; must define cortex-m type!"
#endif

#if __MPU_PRESENT
#if (__CORTEX_M == 0) || (__CORTEX_M == 3) || (__CORTEX_M == 4) || (__CORTEX_M == 7)

void
arm_mpu_init(void)
{
    ARM_MPU_Region_t regions[8] = { 0 };
    size_t size = min(ARRAY_SIZE(regions), mpu_regions_size());

    if (size == 0) {
        return;
    }

    ARM_MPU_Disable();

    for (size_t i = 0; i < size; ++i) {
        regions[i].RBAR = (mpu_regions[i].RBAR & ~MPU_RBAR_REGION_Msk) | i;
        regions[i].RASR = mpu_regions[i].RASR;
    }

    ARM_MPU_Load(regions, size);

    ARM_MPU_Enable(MPU_CTRL_PRIVDEFENA_Msk);

#if defined(__DCACHE_PRESENT) && (__DCACHE_PRESENT == 1U)
    /*
     * Data cache could be already enabled (i.e. by boot loader) and hold
     * lines of memory that is now non-cacheable.
     */
    if (SCB->CCR & SCB_CCR_DC_Msk) {
        SCB_CleanInvalidateDCache();
    }
#endif
}
#elif (__CORTEX_M == 23) || (__CORTEX_M == 33) || (__CORTEX_M == 34) ||       \
    (__CORTEX_M == 35)
/* TODO: Add support for MPU ARMV8 */
#endif

#endif
