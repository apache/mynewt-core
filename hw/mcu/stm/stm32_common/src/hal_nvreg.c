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

#if defined(RTC_BACKUP_SUPPORT) && defined(HAL_PWR_MODULE_ENABLED)
#define PWR_ENABLED 1
#endif

#if PWR_ENABLED
#define HAL_NVREG_MAX (RTC_BKP_NUMBER)
#else
#define HAL_NVREG_MAX (0)
#endif

/* RTC backup registers are 32-bits wide */
#define HAL_NVREG_WIDTH_BYTES (4)

#if PWR_ENABLED
static volatile uint32_t *regs[HAL_NVREG_MAX] = {
#if HAL_NVREG_MAX > 0
    &RTC->BKP0R,
    &RTC->BKP1R,
    &RTC->BKP2R,
    &RTC->BKP3R,
    &RTC->BKP4R,
#endif /* HAL_NVREG_MAX > 0 */
#if HAL_NVREG_MAX > 5
    &RTC->BKP5R,
    &RTC->BKP6R,
    &RTC->BKP7R,
    &RTC->BKP8R,
    &RTC->BKP9R,
    &RTC->BKP10R,
    &RTC->BKP11R,
    &RTC->BKP12R,
    &RTC->BKP13R,
    &RTC->BKP14R,
    &RTC->BKP15R,
#endif /* HAL_NVREG_MAX > 5 */
#if HAL_NVREG_MAX > 16
    &RTC->BKP16R,
    &RTC->BKP17R,
    &RTC->BKP18R,
    &RTC->BKP19R,
#endif /* HAL_NVREG_MAX > 16 */
#if HAL_NVREG_MAX > 20
    &RTC->BKP20R,
    &RTC->BKP21R,
    &RTC->BKP22R,
    &RTC->BKP23R,
    &RTC->BKP24R,
    &RTC->BKP25R,
    &RTC->BKP26R,
    &RTC->BKP27R,
    &RTC->BKP28R,
    &RTC->BKP29R,
    &RTC->BKP30R,
    &RTC->BKP31R,
#endif /* HAL_NVREG_MAX > 20 */
};
#endif /* PWR_ENABLED */

void
hal_nvreg_write(unsigned int reg, uint32_t val)
{
#if PWR_ENABLED
    if (reg < HAL_NVREG_MAX) {
        HAL_PWR_EnableBkUpAccess();
        *regs[reg] = val;
        HAL_PWR_DisableBkUpAccess();
    }
#endif
}

uint32_t
hal_nvreg_read(unsigned int reg)
{
    uint32_t val = 0;
#if PWR_ENABLED
    if (reg < HAL_NVREG_MAX) {
        HAL_PWR_EnableBkUpAccess();
        val = *regs[reg];
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
