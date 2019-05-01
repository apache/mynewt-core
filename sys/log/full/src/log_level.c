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

#include "os/mynewt.h"

#if MYNEWT_VAL(LOG_MODULE_LEVELS)

#include "log/log.h"

/**
 * Contains the minimum log level for each of the 256 modules.  Two levels are
 * packed into a single byte, one per nibble.
 */
static uint8_t log_level_map[(LOG_MODULE_MAX + 1) / 2];

uint8_t
log_level_get(uint8_t module)
{
    uint8_t byte;

    byte = log_level_map[module / 2];
    if (module % 2 == 0) {
        return byte & 0x0f;
    } else {
        return byte >> 4;
    }
}

int
log_level_set(uint8_t module, uint8_t level)
{
    uint8_t *byte;

    if (level > LOG_LEVEL_MAX) {
        level = LOG_LEVEL_MAX;
    }

    byte = &log_level_map[module / 2];
    if (module % 2 == 0) {
        *byte = (*byte & 0xf0) | level;
    } else {
        *byte = (*byte & 0x0f) | (level << 4);
    }

    return 0;
}

#endif
