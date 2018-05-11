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

/*
 * Flash circular buffer.
 */
#include <inttypes.h>
#include <limits.h>

#include "os/mynewt.h"
#include "flash_map/flash_map.h"

#define FCB_MAX_LEN	(CHAR_MAX | CHAR_MAX << 7) /* Max length of element */

/*
 * Entry location is pointer to area (within fcb->f_sectors), and offset
 * within that area.
 */
struct fcb_entry {
    struct flash_area *fe_area;	/* ptr to area within fcb->f_sectors */
    uint32_t fe_elem_off;	/* start of entry */
    uint32_t fe_data_off;	/* start of data */
    uint16_t fe_data_len;	/* size of data area */
};

struct fcb {
    /* Caller of fcb_init fills this in */
    uint32_t f_magic;		/* As placed on the disk */
    uint8_t f_version;  	/* Current version number of the data */
    uint8_t f_sector_cnt;	/* Number of elements in sector array */
    uint8_t f_scratch_cnt;	/* How many sectors should be kept empty */
    struct flash_area *f_sectors; /* Array of sectors, must be contiguous */

    /* Flash circular buffer internal state */
    struct os_mutex f_mtx;	/* Locking for accessing the FCB data */
    struct flash_area *f_oldest;
    struct fcb_entry f_active;
    uint16_t f_active_id;
    uint8_t f_align;		/* writes to flash have to aligned to this */
};

/*
 * Error codes.
 */
#define FCB_OK		0
#define FCB_ERR_ARGS	-1
#define FCB_ERR_FLASH	-2
#define FCB_ERR_NOVAR   -3
#define FCB_ERR_NOSPACE	-4
#define FCB_ERR_NOMEM	-5
#define FCB_ERR_CRC	-6
#define FCB_ERR_MAGIC   -7
#define FCB_ERR_VERSION -8

int fcb_init(struct fcb *fcb);

/*
 * fcb_log is needed as the number of entries in a log
 */
struct fcb_log {
    struct fcb fl_fcb;
    uint8_t fl_entries;
};

/*
 * fcb_append() appends an entry to circular buffer. When writing the
 * contents for the entry, use loc->fl_area and loc->fl_data_off with
 * flash_area_write(). When you're finished, call fcb_append_finish() with
 * loc as argument.
 */
int fcb_append(struct fcb *, uint16_t len, struct fcb_entry *loc);
int fcb_append_finish(struct fcb *, struct fcb_entry *append_loc);

/*
 * Walk over all log entries in FCB, or entries in a given flash_area.
 * cb gets called for every entry. If cb wants to stop the walk, it should
 * return non-zero value.
 *
 * Entry data can be read using flash_area_read(), using
 * loc->fe_area, loc->fe_data_off, and loc->fe_data_len as arguments.
 */
typedef int (*fcb_walk_cb)(struct fcb_entry *loc, void *arg);
int fcb_walk(struct fcb *, struct flash_area *, fcb_walk_cb cb, void *cb_arg);
int fcb_getnext(struct fcb *, struct fcb_entry *loc);

/*
 * Erases the data from oldest sector.
 */
int fcb_rotate(struct fcb *);

/*
 * Start using the scratch block.
 */
int fcb_append_to_scratch(struct fcb *);

/*
 * How many sectors are unused.
 */
int fcb_free_sector_cnt(struct fcb *fcb);

/*
 * Whether FCB has any data.
 */
int fcb_is_empty(struct fcb *fcb);

/*
 * Element at offset *entries* from last position (backwards).
 */
int
fcb_offset_last_n(struct fcb *fcb, uint8_t entries,
        struct fcb_entry *last_n_entry);

/*
 * Clears FCB passed to it
 */
int fcb_clear(struct fcb *fcb);

#ifdef __cplusplus
}
#endif

#endif /* __SYS_FLASHVAR_H_ */
