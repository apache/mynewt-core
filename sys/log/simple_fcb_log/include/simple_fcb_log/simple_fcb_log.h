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

#ifndef H_SIMPLE_FCB_LOG_
#define H_SIMPLE_FCB_LOG_

#include <os/mynewt.h>
#include <log/log.h>

#ifdef __cplusplus
extern "C" {
#endif

#if MYNEWT_VAL(LOG_FCB)
struct simple_fcb_log {
    struct log log;
    struct fcb_log fcb_log;
    /** Number of sectors in sectors array */
    uint32_t sector_count;
    /** Sectors used by FCB */
    struct flash_area *sectors;
};

#elif MYNEWT_VAL(LOG_FCB2)

struct simple_fcb_log {
    struct log log;
    struct fcb_log fcb_log;
    struct flash_sector_range sectors;
};

#endif

/**
 * Initialize and add simple FCB or FCB2 based log
 *
 * @param simplelog - simple log to initialize and register
 * @param flash_area_id - flash area to be used for storage
 * @param log_name - log name
 * @param fcb_magic - value that is stored in every sector to allow verification that
 *                    sector belongs to fcb log
 * @return 0 on success, non-zero on failure
 */
int simple_fcb_log_register(struct simple_fcb_log *simplelog, int flash_area_id,
                            const char *log_name, uint32_t fcb_magic);

#ifdef __cplusplus
}
#endif

#endif
