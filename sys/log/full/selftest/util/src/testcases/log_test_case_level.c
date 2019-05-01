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

#include "log_test_util/log_test_util.h"

static int
log_last_walk(struct log *log, struct log_offset *log_offset,
              void *dptr, uint16_t len)
{
    uint32_t *idx;

    idx = log_offset->lo_arg;
    *idx = log_offset->lo_index;

    return 0;
}

static uint32_t
log_last(struct log *log)
{
    struct log_offset lo;
    uint32_t idx;

    idx = 0;

    lo.lo_arg = &idx;
    log_walk(log, log_last_walk, &lo);

    return idx;
}

TEST_CASE_SELF(log_test_case_level)
{
    struct cbmem cbmem;
    struct log log;
    uint32_t idx;
    int rc;
    int i;

    ltu_setup_cbmem(&cbmem, &log);

    /* Ensure all modules initialized to 0. */
    for (i = 0; i < 256; i++) {
        TEST_ASSERT(log_level_get(i) == 0);
    }

    /* Level too great.  Ensure saturation at LOG_LEVEL_MAX. */
    rc = log_level_set(100, LOG_LEVEL_MAX + 1);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(log_level_get(100) == LOG_LEVEL_MAX);

    /* Ensure all modules can be configured. */
    for (i = 0; i < 256; i++) {
        rc = log_level_set(i, i % 16);
        TEST_ASSERT(rc == 0);
    }
    for (i = 0; i < 256; i++) {
        TEST_ASSERT(log_level_get(i) == i % 16);
    }

    /* Ensure no log write when level is too low. */
    rc = log_level_set(100, 4);
    TEST_ASSERT(rc == 0);

    idx = log_last(&log);
    log_printf(&log, 100, 1, "hello");
    TEST_ASSERT(log_last(&log) == idx);

    /* Ensure log write when level is equal. */
    log_printf(&log, 100, 4, "hello");
    TEST_ASSERT(log_last(&log) > idx);
}
