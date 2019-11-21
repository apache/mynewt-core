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

#ifndef __MCU_FE310_H__
#define __MCU_FE310_H__

/*
 * This is include is just so OS_TICKS_PER_SEC can be taken from
 * syscfg. Do not change it os/mynewt.h.
 */
#include "syscfg/syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OS_TICKS_PER_SEC    MYNEWT_VAL(OS_TICKS_PER_SEC)

static inline void
hal_debug_break(void)
{
    __asm ("ebreak");
}

#ifdef __cplusplus
}
#endif

#endif /* __MCU_FE310_H__ */
