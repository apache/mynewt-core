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

#ifndef __MCU_CORTEX_M33_H_
#define __MCU_CORTEX_M33_H_

#include "mcu/mcu.h"
#include "core_cm33.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void
hal_debug_break(void)
{
    __BKPT(1);
}

static inline void
mcu_mtb_enable(void)
{
    *(uint32_t *)0xe0043004 |= (1 << 31);
    __DSB();
    __ISB();
}

static inline void
mcu_mtb_disable(void)
{
    __ISB();
    *(uint32_t *)0xe0043004 &= ~(1 << 31);
}

#ifdef __cplusplus
}
#endif

#endif /* __MCU_CORTEX_M33_H_ */
