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


/**
 * @addtogroup HAL
 * @{
 *   @defgroup HALDebug HAL Debug
 *   @{
 */

#ifndef H_HAL_DEBUG_
#define H_HAL_DEBUG_

#ifdef __cplusplus
extern "C" {
#endif

#include <syscfg/syscfg.h>

#if MYNEWT_VAL(HAL_BREAK_HOOK)
/* User defined function to be called before code is stopped in debugger */
void hal_break_hook(void);
#else
static inline void hal_break_hook() {}
#endif

#ifndef HAL_DEBUG_BREAK
#define HAL_DEBUG_BREAK()                                      \
        (void)(MYNEWT_VAL(HAL_ENABLE_SOFTWARE_BREAKPOINTS) &&  \
               hal_debugger_connected() &&                     \
               (hal_break_hook(), 1) &&                        \
               (hal_debug_break(), 1))
#endif

#ifdef __cplusplus
}
#endif

#endif /* H_HAL_DEBUG_ */

/**
 *   @} HALDebug
 * @} HAL
 */
