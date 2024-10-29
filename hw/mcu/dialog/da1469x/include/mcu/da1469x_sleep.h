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

#ifndef __MCU_DA1469X_SLEEP_H_
#define __MCU_DA1469X_SLEEP_H_

#include <stdbool.h>
#include "os/os_time.h"

#ifdef __cplusplus
extern "C" {
#endif

struct da1469x_sleep_cb {
    bool (* enter_sleep)(os_time_t ticks);
    void (* exit_sleep)(bool slept);
};

void da1469x_sleep_cb_register(struct da1469x_sleep_cb *cb);
uint32_t da1469x_sleep_wakeup_ticks_get(void);

#ifdef __cplusplus
}
#endif

#endif  /* __MCU_DA1469X_SLEEP_H_ */
