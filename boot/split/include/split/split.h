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

#ifndef _SPLIT_H__
#define _SPLIT_H__

#include "bootutil/bootutil.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SPLIT_NMGR_OP_SPLIT 0

typedef enum {
    /** Loader only. */
    SPLIT_MODE_LOADER =            0,

    /** Loader + app; revert to loader on reboot. */
    SPLIT_MODE_TEST_APP =          1,

    /** Loader + app; no change on reboot. */
    SPLIT_MODE_APP =               2,

    /** Loader only, revert to loader + app on reboot. */
    SPLIT_MODE_TEST_LOADER =       3,
} split_mode_t;

/** Count of valid enum values. */
#define SPLIT_MODE_CNT         4

typedef enum {
    SPLIT_STATUS_INVALID =      0,
    SPLIT_STATUS_NOT_MATCHING = 1,
    SPLIT_STATUS_MATCHING =     2,
} split_status_t;

/*
  * Initializes the split application library */
void split_app_init(void);

/**
  * checks the split application state.
  * If the application is configured to be run (and valid)
  * returns zero and puts the entry data into entry. NOTE:
  * Entry data is not a function pointer, but a pointer
  * suitable to call hal_system_start
  *
  * If toBoot is true, also performs the necessary steps
  * to prepare to boot.  An application may set toBoot to
  * false and call this function to check whether the split
  * application is bootable.
 *
 * @Returns zero on success, non-zero on failures */
int split_app_go(void **entry, int toboot);

split_status_t split_check_status(void);
split_mode_t split_mode_get(void);
int split_mode_set(split_mode_t split_mode);
int split_app_active_get(void);
void split_app_active_set(int active);

int split_write_split(split_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* _SPLIT_H__ */
