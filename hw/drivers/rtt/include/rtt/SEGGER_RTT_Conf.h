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

#ifndef SEGGER_RTT_CONF_H
#define SEGGER_RTT_CONF_H

#include "syscfg/syscfg.h"
#include "os/os.h"

/* Maximum number of up-buffers (target -> host) available */
#define SEGGER_RTT_MAX_NUM_UP_BUFFERS           (3)
/* Maximum number of down-buffers (host -> target) available */
#define SEGGER_RTT_MAX_NUM_DOWN_BUFFERS         (3)

/* Size of up-buffer for Terminal (console output) */
#define BUFFER_SIZE_UP                          (MYNEWT_VAL(RTT_BUFFER_SIZE_UP))
/* Size of down-buffer for Terminal (console input) */
#define BUFFER_SIZE_DOWN                        (MYNEWT_VAL(RTT_BUFFER_SIZE_DOWN))

/* Mode for default channel (Terminal) */
#define SEGGER_RTT_MODE_DEFAULT                 SEGGER_RTT_MODE_NO_BLOCK_SKIP

/* RTT locking functions */
#define SEGGER_RTT_LOCK()               \
        {                               \
            os_sr_t sr;                 \
            OS_ENTER_CRITICAL(sr);
#define SEGGER_RTT_UNLOCK()             \
            OS_EXIT_CRITICAL(sr);       \
        }

#endif
