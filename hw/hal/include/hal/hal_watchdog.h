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
 *   @defgroup HALWatchdog HAL Watchdog
 *   @{
 */

#ifndef _HAL_WATCHDOG_H_
#define _HAL_WATCHDOG_H_

#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Set a recurring watchdog timer to fire no sooner than in 'expire_secs'
 * seconds. Watchdog should be tickled periodically with a frequency
 * smaller than 'expire_secs'. Watchdog needs to be then started with
 * a call to :c:func:`hal_watchdog_enable()`.
 *
 * @param expire_msecs		Watchdog timer expiration time in msecs
 *
 * @return			< 0 on failure; on success return the actual
 *                              expiration time as positive value
 */
int hal_watchdog_init(uint32_t expire_msecs);

/**
 * Starts the watchdog.
 */
void hal_watchdog_enable(void);

/**
 * Tickles the watchdog.   This needs to be done periodically, before
 * the value configured in :c:func:`hal_watchdog_init()` expires.
 */
void hal_watchdog_tickle(void);

#ifdef __cplusplus
}
#endif

#endif /* _HAL_WATCHDOG_H_ */

/**
 *   @} HALWatchdog
 * @} HAL
 */
