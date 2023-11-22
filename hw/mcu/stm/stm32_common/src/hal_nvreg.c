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

#include <hal/hal_nvreg.h>
#include <mcu/stm32_hal.h>

#if (defined(RTC_BACKUP_SUPPORT) || defined(RTC_BKP_NUMBER)) && defined(HAL_PWR_MODULE_ENABLED)
#define PWR_ENABLED 1
#endif

#if PWR_ENABLED
#define HAL_NVREG_MAX (RTC_BKP_NUMBER)
#else
#define HAL_NVREG_MAX (0)
#endif

/* RTC backup registers are 32-bits wide */
#define HAL_NVREG_WIDTH_BYTES (4)

void
hal_nvreg_write(unsigned int reg, uint32_t val)
{
#if PWR_ENABLED
    RTC_HandleTypeDef hrtc = { .Instance = RTC };
    if (reg < HAL_NVREG_MAX) {
        HAL_PWR_EnableBkUpAccess();
        HAL_RTCEx_BKUPWrite(&hrtc, reg, val);
        HAL_PWR_DisableBkUpAccess();
    }
#endif
}

uint32_t
hal_nvreg_read(unsigned int reg)
{
    uint32_t val = 0;
#if PWR_ENABLED
    RTC_HandleTypeDef hrtc = { .Instance = RTC };
    if (reg < HAL_NVREG_MAX) {
        HAL_PWR_EnableBkUpAccess();
        val = HAL_RTCEx_BKUPRead(&hrtc, reg);
        HAL_PWR_DisableBkUpAccess();
    }
#endif
    return val;
}

unsigned int
hal_nvreg_get_num_regs(void)
{
    return HAL_NVREG_MAX;
}

unsigned int
hal_nvreg_get_reg_width(void)
{
    return HAL_NVREG_WIDTH_BYTES;
}
