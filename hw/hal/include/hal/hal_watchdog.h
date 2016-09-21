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

#ifndef _HAL_WATCHDOG_H_
#define _HAL_WATCHDOG_H_

/*
 * Set a recurring watchdog timer to fire no sooner than in 'expire_secs'
 * seconds. Watchdog should be tickled periodically with a frequency
 * smaller than 'expire_secs'. Watchdog needs to be then started with
 * a call to hal_watchdog_enable().
 *
 * @param expire_secs		Watchdog timer expiration time
 *
 * @return			< 0 on failure; on success return the actual
 *                              expiration time as positive value
 */
int hal_watchdog_init(int expire_secs);

/*
 * Starts the watchdog.
 *
 * @return			0 on success; non-zero on failure.
 */
int hal_watchdog_enable(void);

/*
 * Stops the watchdog.
 *
 * @return			0 on success; non-zero on failure.
 */
int hal_watchdog_stop(void);

/*
 * Tickles the watchdog. Needs to be done before 'expire_secs' fires.
 */
void hal_watchdog_tickle(void);

#endif /* _HAL_WATCHDOG_H_ */
