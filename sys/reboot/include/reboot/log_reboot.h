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
#ifndef __LOG_REBOOT_H__
#define __LOG_REBOOT_H__

#define  SOFT_REBOOT (0)
#define  HARD_REBOOT (1)
#define  GEN_CORE    (2)

#define REBOOT_REASON_STR(reason) \
    (SOFT_REBOOT  == reason ? "SOFT"     :\
    (HARD_REBOOT  == reason ? "HARD"     :\
    (GEN_CORE     == reason ? "GEN_CORE" :\
     "UNKNOWN")))

int reboot_init_handler(int log_type, uint8_t entries);
int log_reboot(int reason);

#endif /* _LOG_REBOOT_H__ */
