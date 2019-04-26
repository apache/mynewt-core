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

/**
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

/**
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

/** An individual fcb log bookmark. */
struct fcb_log_bmark {
    /* FCB entry that the bookmark points to. */
    struct fcb_entry flb_entry;

    /* The index of the log entry that the FCB entry contains. */
    uint32_t flb_index;
};

/** A set of fcb log bookmarks. */
struct fcb_log_bset {
    /** Array of bookmarks. */
    struct fcb_log_bmark *fls_bmarks;

    /** The maximum number of bookmarks. */
    int fls_cap;

    /** The number of currently usable bookmarks. */
    int fls_size;

    /** The index where the next bookmark will get written. */
    int fls_next;
};

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
#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
    struct fcb_log_bset fl_bset;
#endif
};

/**
 * fcb_append() appends an entry to circular buffer. When writing the
 * contents for the entry, use loc->fl_area and loc->fl_data_off with
 * flash_area_write(). When you're finished, call fcb_append_finish() with
 * loc as argument.
 */
int fcb_append(struct fcb *, uint16_t len, struct fcb_entry *loc);
int fcb_append_finish(struct fcb *, struct fcb_entry *append_loc);

/**
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
 * Clears FCB passed to it
 */
int fcb_clear(struct fcb *fcb);

/**
 * Usage report for a given FCB area. Returns number of elements and the
 * number of bytes stored in them.
 */
int fcb_area_info(struct fcb *fcb, struct flash_area *fa, int *elemsp,
                  int *bytesp);


#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)

/**
 * Bookmarks are an optimization to speed up lookups in FCB-backed logs.  The
 * concept is simple: maintain a set of flash area+offset pairs corresponding
 * to recently found log entries.  When we perform a log lookup, the walk
 * starts from the bookmark closest to our desired entry rather than from the
 * beginning of the log.
 *
 * Bookmarks are stored in a circular buffer in the fcb_log object.  Each time
 * the log is walked, the starting point of the walk is added to the set of
 * bookmarks. 
 *
 * FCB rotation invalidates all bookmarks.  It is up to the client code to
 * clear a log's bookmarks whenever rotation occurs.
 */

/**
 * @brief Configures an fcb_log to use the specified buffer for bookmarks.
 *
 * @param fcb_log               The log to configure.
 * @param bmarks                The buffer to use for bookmarks.
 * @param bmark_count           The bookmark capacity of the supplied buffer.
 */
void fcb_log_init_bmarks(struct fcb_log *fcb_log,
                         struct fcb_log_bmark *buf, int bmark_count);

/**
 * @brief Erases all bookmarks from the supplied fcb_log.
 *
 * @param fcb_log               The fcb_log to clear.
 */
void fcb_log_clear_bmarks(struct fcb_log *fcb_log);

/**
 * @brief Searches an fcb_log for the closest bookmark that comes before or at
 * the specified index.
 *
 * @param fcb_log               The log to search.
 * @param index                 The index to look for.
 *
 * @return                      The closest bookmark on success;
 *                              NULL if the log has no applicable bookmarks.
 */
const struct fcb_log_bmark *
fcb_log_closest_bmark(const struct fcb_log *fcb_log, uint32_t index);

/**
 * Inserts a bookmark into the provided log.
 *
 * @param fcb_log               The log to insert a bookmark into.
 * @param entry                 The entry the bookmark should point to.
 * @param index                 The log entry index of the bookmark.
 */
void fcb_log_add_bmark(struct fcb_log *fcb_log, const struct fcb_entry *entry,
                       uint32_t index);
#endif

#ifdef __cplusplus
}

/**
 * @} FCB
 */

#endif

#endif /* __SYS_FLASHVAR_H_ */
