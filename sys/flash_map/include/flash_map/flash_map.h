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

#ifndef H_UTIL_FLASH_MAP_
#define H_UTIL_FLASH_MAP_

/**
 *
 * Provides abstraction of flash regions for type of use.
 * I.e. dude where's my image?
 *
 * System will contain a map which contains flash areas. Every
 * region will contain flash identifier, offset within flash and length.
 *
 * 1. This system map could be in a file within filesystem (Initializer
 * must know/figure out where the filesystem is at).
 * 2. Map could be at fixed location for project (compiled to code)
 * 3. Map could be at specific place in flash (put in place at mfg time).
 *
 * Note that the map you use must be valid for BSP it's for,
 * match the linker scripts when platform executes from flash,
 * and match the target offset specified in download script.
 */
#include <stdbool.h>
#include <inttypes.h>

#include "syscfg/syscfg.h"

#ifdef __cplusplus
extern "C" {
#endif

struct flash_area {
    uint8_t fa_id;
    uint8_t fa_device_id;
    uint16_t pad16;
    uint32_t fa_off;
    uint32_t fa_size;
};

struct flash_sector_range {
    struct flash_area fsr_flash_area;
    uint32_t fsr_range_start;
    uint16_t fsr_first_sector;
    uint16_t fsr_sector_count;
    uint32_t fsr_sector_size;
    uint32_t fsr_align;
};

extern const struct flash_area *flash_map;
extern int flash_map_entries;

/*
 * Initializes flash map. Memory will be referenced by flash_map code
 * from this on.
 */
void flash_map_init(void);

/*
 * Start using flash area.
 */
int flash_area_open(uint8_t id, const struct flash_area **);

/** nothing to do for now */
#define flash_area_close(flash_area)

/*
 * Read/write/erase. Offset is relative from beginning of flash area.
 */
int flash_area_read(const struct flash_area *, uint32_t off, void *dst,
  uint32_t len);
int flash_area_write(const struct flash_area *, uint32_t off, const void *src,
  uint32_t len);
int flash_area_erase(const struct flash_area *, uint32_t off, uint32_t len);

/*
 * Whether the whole area is empty.
 */
int flash_area_is_empty(const struct flash_area *, bool *);

/*
 * Reads data. Return code indicates whether it thinks that
 * underlying area is in erased state.
 *
 * Returns 1 if empty, 0 if not. <0 in case of an error.
 */
int flash_area_read_is_empty(const struct flash_area *, uint32_t off, void *dst,
  uint32_t len);

/*
 * Alignment restriction for flash writes.
 */
uint32_t flash_area_align(const struct flash_area *fa);

/*
 * Value read from flash when it is erased.
 */
uint32_t flash_area_erased_val(const struct flash_area *fa);

/*
 * Given flash map index, return info about sectors within the area.
 */
int flash_area_to_sectors(int idx, int *cnt, struct flash_area *ret);

/*
 * Given flash map area id, return info about sectors within the area.
 */
int flash_area_to_sector_ranges(int id, int *cnt,
  struct flash_sector_range *ret);

/*
 * Get-next interface for obtaining info about sectors.
 * To start the get-next walk, call with *sec_id set to -1.
 */
int flash_area_getnext_sector(int id, int *sec_id, struct flash_area *ret);

int flash_area_id_from_image_slot(int slot);
int flash_area_id_to_image_slot(int area_id);

#if MYNEWT_VAL(SELFTEST)
/**
 * Adds areas from the hardcoded flash map that aren't present in, and don't
 * overlap with, the manufacturing flash map.  Only exposed to unit tests.
 */
void flash_map_add_new_dflt_areas_extern(void);
#endif

#ifdef __cplusplus
}
#endif

#endif /* H_UTIL_FLASH_MAP_ */
