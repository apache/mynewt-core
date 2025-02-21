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
#ifndef __SYS_LOG_FCB_H_
#define __SYS_LOG_FCB_H_

#ifdef __cplusplus
extern "C" {
#endif

#if MYNEWT_VAL(LOG_FCB)
#include <fcb/fcb.h>
#elif MYNEWT_VAL(LOG_FCB2)
#include <fcb/fcb2.h>
#endif

/** An individual fcb log bookmark. */
struct log_fcb_bmark {
    /* FCB entry that the bookmark points to. */
#if MYNEWT_VAL(LOG_FCB)
    struct fcb_entry lfb_entry;
#elif MYNEWT_VAL(LOG_FCB2)
    struct fcb2_entry lfb_entry;
#endif
    /* The index of the log entry that the FCB entry contains. */
    uint32_t lfb_index;
};

/** A set of fcb log bookmarks. */
struct log_fcb_bset {
    /** Array of bookmarks. */
    struct log_fcb_bmark *lfs_bmarks;

    /** Enable sector bookmarks */
    bool lfs_en_sect_bmarks;

    /** The maximum number of bookmarks. */
    int lfs_cap;

    /** The number of currently used non-sector bookmarks. */
    int lfs_non_sect_size;

    /** The number of currently usable bookmarks. */
    int lfs_size;

    /** The index where the next non-sector bmark will get written */
    uint32_t lfs_next_non_sect;
};

/**
 * fcb_log is needed as the number of entries in a log
 */
#if MYNEWT_VAL(LOG_FCB)

struct fcb_log {
    struct fcb fl_fcb;
    uint8_t fl_entries;

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    /* Internal - tracking storage use */
    uint32_t fl_watermark_off;
#endif
#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
    struct log_fcb_bset fl_bset;
#endif
    struct log *fl_log;
};

#elif MYNEWT_VAL(LOG_FCB2)

struct fcb_log {
    struct fcb2 fl_fcb;
    uint8_t fl_entries;

#if MYNEWT_VAL(LOG_STORAGE_WATERMARK)
    uint16_t fl_watermark_sec;
    uint32_t fl_watermark_off;
#endif
#if MYNEWT_VAL(LOG_FCB_BOOKMARKS)
    struct log_fcb_bset fl_bset;
#endif
    struct log *fl_log;
};
#endif

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
 * FCB rotation invalidates all bookmarks. It is up to the client code to
 * clear a log's bookmarks whenever rotation occurs.
 */

/**
 * @brief Configures an fcb_log to use the specified buffer for bookmarks.
 *        If sector bookmarks are enabled, buffer should be big enough
 *        to accommodate bookmarks for the entire flash area that is allocated
 *        for the FCB log, i,e; sizeof(struct log_fcb_bmark) *
 *        my_log.fl_fcb.f_sector_cnt + MYNEWT_VAL(LOG_FCB_NUM_ABS_BOOKMARKS)
 *
 * @param fcb_log               The log to configure.
 * @param buf                   The buffer to use for bookmarks.
 * @param bmark_count           The bookmark capacity of the supplied buffer.
 * @param en_sect_bmarks        Enable sector bookmarks
 *
 * @return 0 on success, non-zero on failure
 */
int log_fcb_init_bmarks(struct fcb_log *fcb_log,
                        struct log_fcb_bmark *buf, int bmark_count,
                        bool en_sect_bmarks);

/** @brief Remove bookmarks which point to oldest FCB/FCB2 area. This is
 * meant to get called just before the area is rotated out.
 *
 * @param fcb_log               The fcb_log to operate on.
 */
void log_fcb_rotate_bmarks(struct fcb_log *fcb_log);

/**
 * @brief Erases all bookmarks from the supplied fcb_log.
 *
 * @param fcb_log               The fcb_log to clear.
 */
void log_fcb_clear_bmarks(struct fcb_log *fcb_log);

/**
 * @brief Get bookmarks for a particular log
 *
 * @param log Pointer to the log we want to read bookmarks from
 * @param bmarks_size Pointer to the variable we want to read bookmarks into
 *
 * @return Pointer to the bookmarks array for the provided log
 */
struct log_fcb_bmark *log_fcb_get_bmarks(struct log *log, uint32_t *bmarks_size);

/**
 * @brief Searches an fcb_log for the closest bookmark that comes before or at
 * the specified index.
 *
 * @param fcb_log               The log to search.
 * @param index                 The index to look for.
 * @param min_diff              If bookmark was found, fill it up with the difference
 *                              else it will return -1.
 *
 * @return                      The closest bookmark on success;
 *                              NULL if the log has no applicable bookmarks.
 */
struct log_fcb_bmark *
log_fcb_closest_bmark(struct fcb_log *fcb_log, uint32_t index, int *min_diff);

/**
 * Inserts a bookmark into the provided log.
 *
 * @param fcb_log               The log to insert a bookmark into.
 * @param entry                 The entry the bookmark should point to.
 * @param index                 The log entry index of the bookmark.
 * @paran sect_bmark            Bool indicating it is a sector bookmark.
 */
#if MYNEWT_VAL(LOG_FCB)
void log_fcb_add_bmark(struct fcb_log *fcb_log, struct fcb_entry *entry,
                       uint32_t index, bool sect_bmark);
#elif MYNEWT_VAL(LOG_FCB2)
void log_fcb_add_bmark(struct fcb_log *fcb_log, struct fcb2_entry *entry,
                       uint32_t index, bool sect_bmark);
#endif
#endif

#ifdef __cplusplus
}
#endif

#endif /* __SYS_LOG_FCB_H_ */
