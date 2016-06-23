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

#ifndef __HAL_BSP_H_
#define __HAL_BSP_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>

/*
 * Initializes BSP; registers flash_map with the system.
 */
void bsp_init(void);

/*
 * Return pointer to flash device structure, given BSP specific
 * flash id.
 */
struct hal_flash;
const struct hal_flash *bsp_flash_dev(uint8_t flash_id);

/*
 * Returns which flash map slot the currently running image is at.
 */
int bsp_imgr_current_slot(void);

/*
 * Grows heap by given amount. XXX giving space back not implemented.
 */
void *_sbrk(int incr);

/*
 * Report which memory areas should be included inside a coredump.
 */
struct bsp_mem_dump {
    void *bmd_start;
    uint32_t bmd_size;
};

const struct bsp_mem_dump *bsp_core_dump(int *area_cnt);

/*
 * Get unique HW identifier/serial number for platform.
 * Returns the number of bytes filled in.
 */
int bsp_hw_id(uint8_t *id, int max_len);

#ifdef __cplusplus
}
#endif

#endif
