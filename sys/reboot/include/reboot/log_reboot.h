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
#ifndef __LOG_REBOOT_H__
#define __LOG_REBOOT_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <hal/hal_system.h>
#define REBOOT_REASON_STR(reason)                                       \
    (reason == HAL_RESET_POR ? "HARD" :                                 \
      (reason == HAL_RESET_PIN ? "RESET_PIN" :                          \
        (reason == HAL_RESET_WATCHDOG ? "WDOG" :                        \
          (reason == HAL_RESET_SOFT ? "SOFT" :                          \
            (reason == HAL_RESET_BROWNOUT ? "BROWNOUT" :                \
              (reason == HAL_RESET_REQUESTED ? "REQUESTED" :            \
                "UNKNOWN"))))))

int reboot_init_handler(int log_store_type, uint8_t entries);
int log_reboot(enum hal_reset_reason);
void reboot_start(enum hal_reset_reason reason);

extern uint16_t reboot_cnt;

#ifdef __cplusplus
}
#endif

#endif /* _LOG_REBOOT_H__ */
