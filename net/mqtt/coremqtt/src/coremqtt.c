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

#include <stdarg.h>
#include <stdio.h>
#include "os/mynewt.h"
#include "modlog/modlog.h"
#include "coremqtt/coremqtt.h"

uint32_t
coremqtt_time_ms(void)
{
    return (uint32_t)(os_get_uptime_usec() / 1000);
}

void
coremqtt_log(uint8_t level, const char *fmt, ...)
{
    char buf[MYNEWT_VAL(COREMQTT_LOG_BUF_SIZE)];
    va_list args;
    int len;

    va_start(args, fmt);
    /* Leave room for the newline */
    len = vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);

    if (len < 0) {
        return;
    }
    if (len > (int)sizeof(buf) - 2) {
        len = sizeof(buf) - 2;
    }
    /* Some coreMQTT messages already end with a newline */
    if (len == 0 || buf[len - 1] != '\n') {
        buf[len++] = '\n';
    }

    modlog_append(MYNEWT_VAL(COREMQTT_LOG_MOD), level, LOG_ETYPE_STRING, buf, len);
}
