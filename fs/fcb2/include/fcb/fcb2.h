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
#ifndef __SYS_FCB2_H_
#define __SYS_FCB2_H_

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \defgroup FCB2 Flash circular buffer.
 * @{
 */

#include <inttypes.h>
#include <limits.h>
#include <assert.h>

#include <syscfg/syscfg.h>
#include <os/os_mutex.h>
#include <flash_map/flash_map.h>

#define FCB2_MAX_LEN	(CHAR_MAX | CHAR_MAX << 7) /* Max length of element */

#define FCB2_SECTOR_OLDEST UINT16_MAX

struct fcb2;

/**
 * Entry location point to sector, and offset  within that sector.
 */
struct fcb2_entry {
    struct flash_sector_range *fe_range;  /* ptr to area within fcb->f_ranges */
    uint16_t fe_sector;     /* sector number in fcb flash */
    uint16_t fe_data_len;   /* size of data area */
    uint32_t fe_data_off;   /* start of data in sector */
    uint16_t fe_entry_num;  /* entry number in sector */
};

/* Number of bytes needed for fcb_sector_entry on flash */
#define FCB2_ENTRY_SIZE          6
#define FCB2_CRC_LEN             2

/**
 * State structure for flash circular buffer version2.
 */
struct fcb2 {
    /* Caller of fcb2_init() fills this in */
    uint32_t f_magic;       /* As placed on the disk */
    uint8_t f_version;      /* Current version number of the data */
    uint8_t f_scratch_cnt;  /* How many sectors should be kept empty */
    uint8_t f_range_cnt;    /* Number of elements in range array */
    uint16_t f_sector_cnt;  /* Number of sectors used by fcb */
    uint16_t f_oldest_sec;  /* Index of oldest sector */
    struct flash_sector_range *f_ranges;

    /* Flash circular buffer internal state */
    struct os_mutex f_mtx;	/* Locking for accessing the FCB data */
    struct fcb2_entry f_active;
    uint16_t f_active_id;
};

/**
 * Error codes.
 */
#define FCB2_OK           0
#define FCB2_ERR_ARGS    -1
#define FCB2_ERR_FLASH   -2
#define FCB2_ERR_NOVAR   -3
#define FCB2_ERR_NOSPACE -4
#define FCB2_ERR_NOMEM   -5
#define FCB2_ERR_CRC     -6
#define FCB2_ERR_MAGIC   -7
#define FCB2_ERR_VERSION -8


/**
 *
 * Initialize FCB for use.
 *
 * @param fcb            Fcb to initialize. User specific fields of structure
 *                       must be set before calling this.
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_init(struct fcb2 *fcb);

/**
 * Initialize fcb for specific flash area
 *
 * Function initializes FCB structure with data taken from specified flash
 * area.
 * If FCB was not initialized before in this area, area will be erased.
 *
 * @param fcb            Fcb to initialize
 * @param flash_area_id  flash area for this fcb
 * @param magic          User defined magic value that is stored in each
 *                       flash sector used by fcb
 * @param version        version of fcb
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_init_flash_area(struct fcb2 *fcb, int flash_area_id, uint32_t magic,
                         uint8_t version);

/**
 * fcb2_append() reserves space for an entry within FCB. When writing the
 * contents for the entry, fcb2_write() with fcb_entry filled by.
 * fcb2_append(). When you're finished, call fcb2_append_finish() with
 * loc as argument.
 *
 * @param fcb            FCB this entry is being appended to.
 * @param len            Size of the entry being added
 * @param loc            Will be filled with fcb2_entry where data will be
 *                       written to
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_append(struct fcb2 *fcb, uint16_t len, struct fcb2_entry *loc);

/**
 * Write entry data to location indicated by loc. User must've called
 * fcb2_append() before adding data to entry to populate loc. Should not
 * be called after fcb2_append_finish() for this entry has been called.
 *
 * @param loc            Where this entry will be written
 * @param off            Offset within entry area where to write the data
 * @param buf            Pointer to data being written
 * @param len            Number of bytes to write
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_write(struct fcb2_entry *loc, uint16_t off, const void *buf,
               uint16_t len);

/**
 * Finalize writing data for entry.
 *
 * @param append_loc     Location of the entry
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_append_finish(struct fcb2_entry *append_loc);

/**
 * Callback routine getting called when walking through FCB entries.
 * Entry data can be read by using fcb2_read().
 *
 * @param loc            Location of the entry
 * @param arg            void * which was passed as arg when calling fcb2_walk()
 *
 * @return 0 if walk should continue, non-zero if walk should stop
 */
typedef int (*fcb2_walk_cb)(struct fcb2_entry *loc, void *arg);

/**
 * Iterate over all entries within FCB. Caller passes callback function to
 * call for every valid entry. Entries are walked from oldest to newest.
 *
 * @param fcb            FCB to iterate
 * @param sector         Sector ID where walk should start.
 *                       Use FCB2_SECTOR_OLDEST to start from beginning.
 * @param cb             Callback to call for the entries.
 * @param cb_arg         void * to pass to callback.
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_walk(struct fcb2 *fcb, int sector, fcb2_walk_cb cb, void *cb_arg);

/**
 * Walk through entries within FCB from oldest to newest.
 * fcb_getnext() finds the next valid entry forward from loc, and fills in
 * the location of that entry.
 *
 * @param fcb            FCB to walk
 * @param loc            FCB entry location. To get the oldest entry
 *                       set loc->fe_range to NULL, and fe_entry_num to 0.
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_getnext(struct fcb2 *fcb, struct fcb2_entry *loc);

/**
 * Walk through entries within FCB from newest to oldest.
 * fcb_getprev() finds the previous valid entry backwards from loc, and fills in
 * the location of that entry.
 *
 * @param fcb            FCB to walk
 * @param loc            FCB entry location. To get the newest entry
 *                       set loc->fe_range to NULL.
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_getprev(struct fcb2 *fcb, struct fcb2_entry *loc);

/**
 * Read entry data belonging pointed by loc.
 *
 * @param loc            FCB entry to read
 * @param off            Offset within entry area
 * @param buf            Pointer to area where to populate the data
 * @param len            Number of bytes to read.
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_read(struct fcb2_entry *loc, uint16_t off, void *buf, uint16_t len);

/**
 * Erases the data from oldest sector.
 *
 * @param fcb            FCB where to erase the sector
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_rotate(struct fcb2 *fcb);

/**
 * Start using the scratch block.
 *
 * @param fcb
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_append_to_scratch(struct fcb2 *fcb);

/**
 * How many sectors are unused.
 */
int fcb2_free_sector_cnt(struct fcb2 *fcb);

/**
 * Whether FCB has any data.
 *
 * @param fcb            FCB to inspect
 *
 * @return 1 if area is empty, 0 if not
 */
int fcb2_is_empty(struct fcb2 *fcb);

/**
 * Element at offset *entries* from last position (backwards).
 *
 * @param fcb            FCB to inspect
 * @param entries        How many entries to look backwards
 * @param last_n_entry   location of entry being returned
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_offset_last_n(struct fcb2 *fcb, uint8_t entries,
                       struct fcb2_entry *last_n_entry);

/**
 * Erase sector in FCB
 *
 * @param fcb            FCB to use
 * @param sector         sector number to erase 0..f_sector_cnt
 *
 * return 0 on success, error code on failure
 */
int fcb2_sector_erase(const struct fcb2 *fcb, int sector);

/**
 * Get total size of FCB
 *
 * @param fcb            FCB to inspect
 *
 * @return FCB's size in bytes
 */
int fcb2_get_total_size(const struct fcb2 *fcb);

/**
 * Empty the FCB
 *
 * @param fcb            FCB to clear
 *
 * @return 0 on success. Otherwise one of FCB2_XXX error codes.
 */
int fcb2_clear(struct fcb2 *fcb);

/**
 * Usage report for a given FCB sector. Returns number of elements and the
 * number of bytes stored in them.
 *
 * @param fcb            FCB to inspect
 * @param sector         Sector to inspect within FCB
 * @param elemsp         Pointer where number of elements should be stored
 * @param bytesp         Pointer where number of bytes used should be stored
 */
int fcb2_area_info(struct fcb2 *fcb, int sector, int *elemsp, int *bytesp);

#ifdef __cplusplus
}

/**
 * @} FCB2
 */

#endif

#endif /* __SYS_FLASHVAR_H_ */
