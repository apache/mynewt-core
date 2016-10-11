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

#ifndef __BOOTUTIL_MISC_H_
#define __BOOTUTIL_MISC_H_

#include <inttypes.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /** Loader only. */
    BOOT_SPLIT_MODE_LOADER =            0,

    /** Loader + app; revert to loader on reboot. */
    BOOT_SPLIT_MODE_TEST_APP =          1,

    /** Loader + app; no change on reboot. */
    BOOT_SPLIT_MODE_APP =               2,

    /** Loader only, revert to loader + app on reboot. */
    BOOT_SPLIT_MODE_TEST_LOADER =       3,
} boot_split_mode_t;

/** Count of valid enum values. */
#define BOOT_SPLIT_MODE_CNT         4

extern int8_t boot_split_mode;

int boot_vect_read_test(int *slot);
int boot_vect_read_main(int *slot);
int boot_vect_write_test(int slot);
int boot_vect_write_main(void);

boot_split_mode_t boot_split_mode_get(void);
int boot_split_mode_set(boot_split_mode_t split_mode);

int boot_split_app_active_get(void);
void boot_split_app_active_set(int active);

#ifdef __cplusplus
}
#endif

#endif /*  __BOOTUTIL_MISC_H_ */
