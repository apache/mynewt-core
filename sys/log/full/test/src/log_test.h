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
#ifndef _LOG_TEST_H
#define _LOG_TEST_H
#include <string.h>

#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "fcb/fcb.h"
#include "log/log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FCB_FLASH_AREAS 2

extern struct flash_area fcb_areas[FCB_FLASH_AREAS];

extern struct fcb log_fcb;
extern struct log my_log;

#define FCB_STR_LOGS_CNT 3

extern char *str_logs[FCB_STR_LOGS_CNT];

extern int str_idx;
extern int str_max_idx;

int log_test_walk1(struct log *log, struct log_offset *log_offset,
                   void *dptr, uint16_t len);
int log_test_walk2(struct log *log, struct log_offset *log_offset,
                   void *dptr, uint16_t len);

#ifdef __cplusplus
}
#endif

#endif /* _LOG_TEST_H */
