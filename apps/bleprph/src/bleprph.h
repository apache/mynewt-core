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

#ifndef H_BLEPRPH_
#define H_BLEPRPH_

#include "log/log.h"

void gatt_svr_init(void);

extern struct log bleprph_log;

extern const char *bleprph_device_name;
extern const uint16_t bleprph_appearance;
extern const uint8_t bleprph_privacy_flag;
extern uint8_t bleprph_reconnect_addr[6];
extern uint8_t bleprph_pref_conn_params[8];
extern uint8_t bleprph_gatt_service_changed[4];

/* bleprph uses the first "peruser" log module. */
#define BLEPRPH_LOG_MODULE  (LOG_MODULE_PERUSER + 0)

/* Convenience macro for logging to the bleprph module. */
#define BLEPRPH_LOG(lvl, ...) \
    LOG_ ## lvl(&bleprph_log, BLEPRPH_LOG_MODULE, __VA_ARGS__)

#endif
