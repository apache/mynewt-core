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
#include "log_test.h"

TEST_CASE(log_setup_fcb)
{
    int rc;
    int i;

    log_fcb.f_sectors = fcb_areas;
    log_fcb.f_sector_cnt = sizeof(fcb_areas) / sizeof(fcb_areas[0]);
    log_fcb.f_magic = 0x7EADBADF;
    log_fcb.f_version = 0;

    for (i = 0; i < log_fcb.f_sector_cnt; i++) {
        rc = flash_area_erase(&fcb_areas[i], 0, fcb_areas[i].fa_size);
        TEST_ASSERT(rc == 0);
    }
    rc = fcb_init(&log_fcb);
    TEST_ASSERT(rc == 0);

    log_register("log", &my_log, &log_fcb_handler, &log_fcb, LOG_SYSLEVEL);
}
