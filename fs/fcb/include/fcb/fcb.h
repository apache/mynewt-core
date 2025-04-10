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
#ifndef __SYS_FCB_H_
#define __SYS_FCB_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup FCB Flash circular buffer.
 * @{
 */

#include <inttypes.h>
#include <limits.h>
#include <assert.h>

#include <syscfg/syscfg.h>
#include <os/os_mutex.h>
#include <flash_map/flash_map.h>

#define FCB_MAX_LEN	(CHAR_MAX | CHAR_MAX << 7) /* Max length of element */

struct fcb_entry_cache {
    /* Sector number in FCB */
    uint16_t sector_ix;
    /* Number of entries that cache can keep */
    uint16_t cache_size;
    /* Number of entries in cache */
    uint16_t cache_count;
    /* Cached data */
    uint16_t *cache_data;
};

/**
 * Entry location is pointer to area (within fcb->f_sectors), and offset
 * within that area.
 */
struct fcb_entry {
    struct flash_area *fe_area;	/* ptr to area within fcb->f_sectors */
    uint32_t fe_elem_off;	/* start of entry */
    uint32_t fe_data_off;	/* start of data */
    uint16_t fe_data_len;	/* size of data area */
    uint16_t fe_elem_ix;    /* Element index in current sector */
    struct fcb_entry_cache *fe_cache;
    bool fe_step_back; /* walk backwards */
};

struct fcb {
    /* Caller of fcb_init fills this in */
    uint32_t f_magic;		/* As placed on the disk */
    uint8_t f_version;  	/* Current version number of the data */
    uint16_t f_sector_cnt;	/* Number of elements in sector array */
    uint16_t f_scratch_cnt;	/* How many sectors should be kept empty */
    /** Number of element in active sector (f_active) */
    uint16_t f_active_sector_entry_count;
    struct flash_area *f_sectors; /* Array of sectors, must be contiguous */

    /* Flash circular buffer internal state */
    struct os_mutex f_mtx;	/* Locking for accessing the FCB data */
    struct flash_area *f_oldest;
    struct fcb_entry f_active;
    uint16_t f_active_id;
    uint8_t f_align;		/* writes to flash have to aligned to this */
};

/**
 * Init FCB cache that can be used for walking back optimization.
 *
 * Note: Cache can be used to speed up walk back functionality.
 * Cache uses memory allocated by os_malloc/os_realloc and must
 * be freed after use with fcb_cache_free().
 *
 * To setup cache:
 * struct fcb_entry_cache cache;
 * struct fcb_entry loc = {};
 *
 * loc.fe_cache = &cache;
 * fcb_cache_init(fcb, &cache, 100);
 * loc.fe_step_back = true;
 *
 * fcb_getnext(fcb, &loc);
 *
 * fcb_cache_free(fcb, cache);
 *
 * @param fcb - fcb to init cache for
 * @param cache - cache to init
 * @param initial_entry_count - initial number of entries in the cache
 * @return 0 - on success
 *         SYS_ENOMEM - when cache memory can't be allocated.
 */
int fcb_cache_init(struct fcb *fcb, struct fcb_entry_cache *cache, int initial_entry_count);

/**
 * Free memory allocate by fcb cache.
 *
 * @param fcb - fcb associated with cache
 * @param cache - cache to free
 */
void fcb_cache_free(struct fcb *fcb, struct fcb_entry_cache *cache);

/**
 * Error codes.
 */
#define FCB_OK             0
#define FCB_ERR_ARGS      -1
#define FCB_ERR_FLASH     -2
#define FCB_ERR_NOVAR     -3
#define FCB_ERR_NOSPACE   -4
#define FCB_ERR_NOMEM     -5
#define FCB_ERR_CRC       -6
#define FCB_ERR_MAGIC     -7
#define FCB_ERR_VERSION   -8
#define FCB_ERR_NEXT_SECT -9

int fcb_init(struct fcb *fcb);

/**
 * fcb_append() appends an entry to circular buffer. When writing the
 * contents for the entry, use loc->fl_area and loc->fl_data_off with
 * flash_area_write(). When you're finished, call fcb_append_finish() with
 * loc as argument.
 */
int fcb_append(struct fcb *, uint16_t len, struct fcb_entry *loc);
int fcb_append_finish(struct fcb *, struct fcb_entry *append_loc);

/**
 * Write to flash user data.
 *
 * Function should be called after fcb_append is called and before fcb_finish
 * This is wrapper for flash_area_write() function and uses loc for starting
 * location. loc is modified and can be used for subsequent writes.
 *
 * @param fcb - fcb to write entry to
 * @param loc - location of the entry
 * @param buf - data to write
 * @param len - number of bytes to write to fcb
 * @return 0 on success, non-zero on failure
 */
int fcb_write(struct fcb *fcb, struct fcb_entry *loc, const uint8_t *buf, size_t len);

/**
 * Walk over all entries in FCB.
 * cb gets called for every entry. If cb wants to stop the walk, it should
 * return non-zero value.
 *
 * Entry data can be read using flash_area_read(), using
 * loc->fe_area, loc->fe_data_off, and loc->fe_data_len as arguments.
 */
typedef int (*fcb_walk_cb)(struct fcb_entry *loc, void *arg);
int fcb_walk(struct fcb *, struct flash_area *, fcb_walk_cb cb, void *cb_arg);
int fcb_getnext(struct fcb *, struct fcb_entry *loc);

/**
 * Get first entry in the provided flash area
 *
 * @param fcb Pointer to FCB
 * @param fap Optional pointer to flash area
 * @param loc Pointer to first FCB entry in the provided flash area
 *
 * @return 0 on success, non-zero on failure
 */
int fcb_getnext_in_area(struct fcb *fcb, struct flash_area *fap,
                        struct fcb_entry *loc);

/**
 * Get next area pointer from the FCB pointer
 *
 * @param fcb Pointer to the FCB
 * @param fap Pointer to the flash_area
 *
 * @return Pointer to the flash_area that comes next
 */
struct flash_area *fcb_getnext_area(struct fcb *fcb, struct flash_area *fap);

#if MYNEWT_VAL_FCB_BIDIRECTIONAL
/**
 * Call 'cb' for every element in flash circular buffer moving
 * from the newest to oldest entries.
 *
 * @param fcb - fcb to walk through
 * @param cb - function to call for each entry
 * @param cb_arg - argument to pass to cb
 * @return 0 after whole buffer was traversed
 *         non zero value if cb requested termination of the walk
 */
int fcb_walk_back(struct fcb *fcb, fcb_walk_cb cb, void *cb_arg);
#endif

/**
 * Erases the data from oldest sector.
 */
int fcb_rotate(struct fcb *);

/**
 * Start using the scratch block.
 */
int fcb_append_to_scratch(struct fcb *);

/**
 * How many sectors are unused.
 *
 * @param fcb - fcb to check
 * @return number of free sectors if successful,
 *  negative value on error
 */
int fcb_free_sector_cnt(struct fcb *fcb);

/**
 * Whether FCB has any data.
 *
 * @param fcb - fcb to check
 * @return 1 if FCB is empty, 0 if FCB has data,
 *  negative value on error
 */
int fcb_is_empty(struct fcb *fcb);

/**
 * Element at offset *entries* from last position (backwards).
 */
int
fcb_offset_last_n(struct fcb *fcb, uint8_t entries,
        struct fcb_entry *last_n_entry);

/**
 * Clears FCB passed to it
 */
int fcb_clear(struct fcb *fcb);

/**
 * Usage report for a given FCB area. Returns number of elements and the
 * number of bytes stored in them.
 */
int fcb_area_info(struct fcb *fcb, struct flash_area *fa, int *elemsp,
                  int *bytesp);

#ifdef __cplusplus
}

/**
 * @} FCB
 */

#endif

#endif /* __SYS_FLASHVAR_H_ */

/**
 * @} FCB
 */
