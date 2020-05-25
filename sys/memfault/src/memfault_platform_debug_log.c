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

#include "console/console.h"
#include "memfault/core/platform/debug_log.h"

#include <stdio.h>

#ifndef MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES
#define MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES (128)
#endif

void
memfault_platform_log(eMemfaultPlatformLogLevel level, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    char log_buf[MEMFAULT_DEBUG_LOG_BUFFER_SIZE_BYTES];
    vsnprintf(log_buf, sizeof(log_buf), fmt, args);

    const char *lvl_str = "???";
    switch (level) {
    case kMemfaultPlatformLogLevel_Debug:
        lvl_str = "dbg";
        break;

    case kMemfaultPlatformLogLevel_Info:
        lvl_str = "inf";
        break;

    case kMemfaultPlatformLogLevel_Warning:
        lvl_str = "wrn";
        break;

    case kMemfaultPlatformLogLevel_Error:
        lvl_str = "err";
        break;

    default:
        break;
    }
    console_printf("<%s> <mflt>: %s\n", lvl_str, log_buf);

    va_end(args);
}

void
memfault_platform_log_raw(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
    console_printf("\n");
}
