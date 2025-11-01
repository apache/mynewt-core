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

#include <stdbool.h>
#include <os/mynewt.h>
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

#if MYNEWT_VAL_MCU_STM32F1
#define HAL_NVREG_WIDTH_BYTES 2
#define HAL_NVREG_START_INDEX 1
#else
/* RTC backup registers are 32-bits wide */
#define HAL_NVREG_WIDTH_BYTES (4)
#define HAL_NVREG_START_INDEX 0
#endif

void
hal_nvreg_write(unsigned int reg, uint32_t val)
{
#if PWR_ENABLED
    RTC_HandleTypeDef hrtc = { .Instance = RTC };
    if (reg < HAL_NVREG_MAX) {
#if defined(__HAL_RCC_PWR_IS_CLK_DISABLED)
        bool was_pwr_disabled = __HAL_RCC_PWR_IS_CLK_DISABLED();
        if (was_pwr_disabled) {
            __HAL_RCC_PWR_CLK_ENABLE();
        }
#endif
#if defined(__HAL_RCC_BKP_CLK_ENABLE)
        __HAL_RCC_BKP_CLK_ENABLE();
#endif
        HAL_PWR_EnableBkUpAccess();
        HAL_RTCEx_BKUPWrite(&hrtc, reg + HAL_NVREG_START_INDEX, val);
        HAL_PWR_DisableBkUpAccess();
#if defined(__HAL_RCC_BKP_CLK_DISABLE)
        __HAL_RCC_BKP_CLK_DISABLE();
#endif
#if defined(__HAL_RCC_PWR_IS_CLK_DISABLED)
        if (was_pwr_disabled) {
            __HAL_RCC_PWR_CLK_DISABLE();
        }
#endif
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
        val = HAL_RTCEx_BKUPRead(&hrtc, reg + HAL_NVREG_START_INDEX);
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
