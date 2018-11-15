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

#include "os/mynewt.h"
#include "flash_map/flash_map.h"

#define FCB_MAX_LEN	(CHAR_MAX | CHAR_MAX << 7) /* Max length of element */

#define FCB_SECTOR_OLDEST UINT16_MAX

struct fcb;

/**
 * Entry location point to sector, and offset
 * within that sector.
 */
struct fcb_entry {
    struct fcb *fe_fcb;
    struct flash_sector_range *fe_range;  /* ptr to area within fcb->f_ranages */
    uint16_t fe_sector;     /* sector number in fcb flash */
    uint16_t fe_data_len;   /* size of data area */
    uint32_t fe_data_off;   /* start of data in sector */
    uint16_t fe_entry_num;  /* entry number in sector */
};

/* Number of bytes needed for fcb_sector_entry on flash */
#define FCB_ENTRY_SIZE          6
#define FCB_CRC_LEN             2

struct fcb {
    /* Caller of fcb_init fills this in */
    uint32_t f_magic;       /* As placed on the disk */
    uint8_t f_version;      /* Current version number of the data */
    uint8_t f_scratch_cnt;  /* How many sectors should be kept empty */
    uint8_t f_range_cnt;    /* Number of elements in range array */
    uint16_t f_sector_cnt;  /* Number of sectors used by fcb */
    struct flash_sector_range *f_ranges;

    /* Flash circular buffer internal state */
    struct os_mutex f_mtx;	/* Locking for accessing the FCB data */
    uint16_t f_oldest_sec;
    struct fcb_entry f_active;
    uint16_t f_active_id;
    uint16_t f_sector_entries; /* Number of entries in current sector */
};

struct fcb_sector_info {
    struct flash_sector_range *si_range;  /* Sector range */
    uint32_t si_sector_offset;            /* Sector offset in fcb */
    uint16_t si_sector_in_range;          /* Sector number relative to si_range */
};

/**
 * Error codes.
 */
#define FCB_OK           0
#define FCB_ERR_ARGS    -1
#define FCB_ERR_FLASH   -2
#define FCB_ERR_NOVAR   -3
#define FCB_ERR_NOSPACE -4
#define FCB_ERR_NOMEM   -5
#define FCB_ERR_CRC     -6
#define FCB_ERR_MAGIC   -7
#define FCB_ERR_VERSION -8

int fcb_init(struct fcb *fcb);

/*
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
 */
int fcb_init_flash_area(struct fcb *fcb, int flash_area_id, uint32_t magic,
    uint8_t version);

/**
 * fcb_log is needed as the number of entries in a log
 */
struct fcb_log {
    struct fcb fl_fcb;
    uint8_t fl_entries;

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    /* Internal - tracking storage use */
    uint32_t fl_watermark_off;
#endif
};

/**
 * fcb_append() appends an entry to circular buffer. When writing the
 * contents for the entry, fcb_write() with fcb_entry filled by.
 * fcb_append(). When you're finished, call fcb_append_finish() with
 * loc as argument.
 */
int fcb_append(struct fcb *fcb, uint16_t len, struct fcb_entry *loc);
int fcb_write(struct fcb_entry *loc, uint16_t off, void *buf, uint16_t len);
int fcb_append_finish(struct fcb_entry *append_loc);

/**
 * Walk over all log entries in FCB, or entries in a given flash_area.
 * cb gets called for every entry. If cb wants to stop the walk, it should
 * return non-zero value.
 *
 * Entry data can be read using fcb_read().
 */
typedef int (*fcb_walk_cb)(struct fcb_entry *loc, void *arg);
int fcb_walk(struct fcb *, int sector, fcb_walk_cb cb, void *cb_arg);
int fcb_getnext(struct fcb *fcb, struct fcb_entry *loc);
int fcb_read(struct fcb_entry *loc, uint16_t off, void *buf, uint16_t len);

/**
 * Erases the data from oldest sector.
 */
int fcb_rotate(struct fcb *fcb);

/**
 * Start using the scratch block.
 */
int fcb_append_to_scratch(struct fcb *fcb);

/**
 * How many sectors are unused.
 */
int fcb_free_sector_cnt(struct fcb *fcb);

/**
 * Whether FCB has any data.
 */
int fcb_is_empty(struct fcb *fcb);

/**
 * Element at offset *entries* from last position (backwards).
 */
int
fcb_offset_last_n(struct fcb *fcb, uint8_t entries,
        struct fcb_entry *last_n_entry);

/**
 * Get total size of FCB
 *
 * @param fcb     FCB to use
 *
 * return FCB's size in bytes
 */
int fcb_get_total_size(const struct fcb *fcb);

/**
 * Clears FCB passed to it
 */
int fcb_clear(struct fcb *fcb);

/**
 * Usage report for a given FCB sector. Returns number of elements and the
 * number of bytes stored in them.
 */
int fcb_area_info(struct fcb *fcb, int sector, int *elemsp, int *bytesp);

#ifdef __cplusplus
}

/**
 * @} FCB
 */

#endif

#endif /* __SYS_FLASHVAR_H_ */
