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

#include <fcb/fcb.h>

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
#endif

#endif /* __SYS_LOG_FCB_H_ */
