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

#ifndef H_COREMQTT_
#define H_COREMQTT_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Returns the current time in milliseconds.
 *
 * Can be passed as the time function to MQTT_Init(). The value wraps
 * around after about 49 days, which coreMQTT handles.
 *
 * @return Milliseconds since boot (truncated to 32 bits).
 */
uint32_t coremqtt_time_ms(void);

/**
 * Logs a formatted coreMQTT message to the coreMQTT log module.
 *
 * Used by the coreMQTT logging macros; a newline is appended to every
 * message.
 *
 * @param level  Log level (LOG_LEVEL_*).
 * @param fmt    printf-style format string.
 */
void coremqtt_log(uint8_t level, const char *fmt, ...);

#ifdef __cplusplus
}
#endif

#endif
