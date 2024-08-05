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
#include <stddef.h>

#include "fcb/fcb.h"
#include "fcb_priv.h"
#include "defs/error.h"

#if MYNEWT_VAL_FCB_BIDIRECTIONAL
#define FCB_STEP_BACK(loc) ((loc)->fe_step_back)
#else
#define FCB_STEP_BACK(loc) (false)
#endif
#if MYNEWT_VAL_FCB_BIDIRECTIONAL && MYNEWT_VAL_FCB_BIDIRECTIONAL_CACHE
#define FCB_ENTRY_CACHE(loc) ((loc)->fe_cache)
#define FCB_ENTRY_CACHE_DATA(loc) ((loc)->fe_cache->cache_data)
#else
#define FCB_ENTRY_CACHE(loc) (NULL)
#define FCB_ENTRY_CACHE_DATA(loc) (NULL)
#endif

/**
 * Return flash sector size
 *
 * @param fcb - fcb to check sector size from
 * @param sector - 0 base index of sector
 * @return sector size
 */
static int
fcb_sector_size(struct fcb *fcb, int sector)
{
    return (int)fcb->f_sectors[sector].fa_size;
}

/**
 * Return offset in flash sector where first entry resides
 *
 * This points past FCB header that is present in each sector.
 *
 * @param fcb - fcb to test
 * @return offset of first entry in sector (aligned according to flash)
 */
static int
fcb_start_offset(struct fcb *fcb)
{
    return fcb_len_in_flash(fcb, sizeof(struct fcb_disk_area));
}

int
fcb_getnext_in_area(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc;

    rc = fcb_elem_info(fcb, loc);
    if (rc == 0 || rc == FCB_ERR_CRC) {
        do {
            loc->fe_elem_off = loc->fe_data_off +
                               fcb_len_in_flash(fcb, loc->fe_data_len) +
                               fcb_len_in_flash(fcb, FCB_CRC_SZ);
            loc->fe_elem_ix++;
            rc = fcb_elem_info(fcb, loc);
        } while (rc == FCB_ERR_CRC);
    }
    return rc;
}

struct flash_area *
fcb_getnext_area(struct fcb *fcb, struct flash_area *fap)
{
    fap++;
    if (fap >= &fcb->f_sectors[fcb->f_sector_cnt]) {
        fap = &fcb->f_sectors[0];
    }
    return fap;
}

static int
fcb_get_sector_ix(struct fcb *fcb, struct flash_area *fa)
{
    return fa - fcb->f_sectors;
}

static struct flash_area *
fcb_get_prev_area(struct fcb *fcb, struct flash_area *fap)
{
    fap--;
    if (fap < fcb->f_sectors) {
        fap = &fcb->f_sectors[fcb->f_sector_cnt - 1];
    }
    return fap;
}

/**
 * Return number of bytes needed to encode entry of given length.
 *
 * Up to 127 bytes are encoded in single byte.
 *
 * @param len
 * @return 1 when len < 0 otherwise 2
 */
static int
fcb_len_bytes(uint16_t len)
{
    return len > 127 ? 2 : 1;
}

int
fcb_entry_total_len(struct fcb *fcb, int len)
{
    return fcb_len_in_flash(fcb, fcb_len_bytes(len)) +
           fcb_len_in_flash(fcb, len) +
           fcb_len_in_flash(fcb, FCB_CRC_SZ);
}

/**
 * Compute total bytes that fcb_entry occupies in flash.
 *
 * @param fcb - fcb to get information from.
 * @param loc - entry information as filled with fcb_getnext()
 * @return number of bytes that entry occupies in flash.
 */
static int
fcb_entry_len_in_flash(struct fcb *fcb, struct fcb_entry *loc)
{
    return fcb_entry_total_len(fcb, loc->fe_data_len);
}

/**
 * Read length of fcb entry that is located at given flash area offset.
 *
 * @param fcb - fcb to read length from
 * @param fa  - flash area to read
 * @param offset - offset of entry to read
 * @return entry length on success
 *         FCB_ERR_NOVAR - when specified location was not written yet
 *         FCB_ERR_FLASH - when flash read occurs (flash is corrupted or
 *                         offset is outside flash area range).
 */
static int
fcb_read_entry_len(struct fcb *fcb, struct flash_area *fa, int offset)
{
    int rc;
    uint8_t buf[2];
    uint16_t len;

    (void)fcb;

    rc = flash_area_read_is_empty(fa, offset, buf, 2);

    if (rc < 0) {
        return FCB_ERR_FLASH;
    } else if (rc == 1) {
        return FCB_ERR_NOVAR;
    }

    fcb_get_len(buf, &len);
    return len;
}

/**
 * Reads entry length from flash and updates fcb_entry
 *
 * fe_data_off and fe_data_len are updated only if record exists in flash.
 *
 * @param fcb - fcb to use
 * @param loc - entry location description to update
 * @return 0 on success
 *         FCB_ERR_NOVAR - when specified location was not written yet
 *         FCB_ERR_FLASH - when flash read occurs (flash is corrupted or
 *                         offset is outside flash area range).
 */
static int
fcb_update_entry_len(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc;

    rc = fcb_read_entry_len(fcb, loc->fe_area, (int)loc->fe_elem_off);

    if (rc >= 0) {
        loc->fe_data_off = loc->fe_elem_off + fcb_len_in_flash(fcb, fcb_len_bytes(rc));
        loc->fe_data_len = rc;
        rc = 0;
    }

    return rc;
}

/**
 * Invalidate entry cache
 *
 * Called when sector change during walk backward.
 *
 * @param fcb - fcb to invalidate cache
 * @param cache - cache to invalidate
 */
static void
fcb_cache_invalidate(struct fcb *fcb, struct fcb_entry_cache *cache)
{
    (void)fcb;

    if (cache) {
        cache->cache_count = 0;
        cache->sector_ix = 0xFFFF;
    }
}

/**
 * Function invalidates cache if sector changes
 *
 * If sector number is different that one that was cached all entries are
 * removed.
 *
 * @param fcb - fcb to invalidate cache
 * @param cache - cache to invalidate if needed
 * @param sector_ix - sector number
 */
static void
fcb_cache_switch_to_sector(struct fcb *fcb, struct fcb_entry_cache *cache, int sector_ix)
{
    if (cache != NULL && cache->sector_ix != sector_ix) {
        fcb_cache_invalidate(fcb, cache);
        cache->sector_ix = sector_ix;
    }
}

/**
 * Retrieve element length from cache
 *
 * @param fcb - fcb to use
 * @param cache - cache to use
 * @param elem_ix - element index
 * @return element length if present in cache
 *         SYS_NOENT if cache does not have element
 */
static int
fcb_cache_elem_len(struct fcb *fcb, struct fcb_entry_cache *cache, int elem_ix)
{
    int len;

    (void)fcb;

    if (cache && cache->cache_count > elem_ix) {
        len = cache->cache_data[elem_ix];
    } else {
        len = SYS_ENOENT;
    }
    return len;
}

/**
 * Store entry information in cache.
 *
 * Stores entry
 * @param fcb - fcb to use
 * @param cache - cache to store information (can be NULL)
 * @param entry_ix - index of entry in cache
 * @param offset - offset of entry
 * @param elem_size - FCB entry size in bytes
 */
static void
fcb_cache_add(struct fcb *fcb, struct fcb_entry_cache *cache, int entry_ix, int offset, uint16_t elem_size)
{
    /* Total bytes for entries in sector */
    int sector_data_size;
    int first_entry_offset;

    if (cache == NULL || cache->cache_data == NULL) {
        return;
    }

    first_entry_offset = fcb_start_offset(fcb);

    /* Data size is size of sector minus sector header */
    sector_data_size = fcb_sector_size(fcb, cache->sector_ix) - first_entry_offset;

    if (entry_ix >= cache->cache_size) {
        assert(cache->cache_count == cache->cache_size);

        int average_entry_size = (offset - first_entry_offset) / entry_ix;
        /*  guess cache size based on entries currently in flash */
        int new_size = cache->cache_size + 5 + (sector_data_size - offset) / average_entry_size;
        size_t cache_byte_count = new_size * sizeof(cache->cache_data[0]);

        cache->cache_data = os_realloc(cache->cache_data, cache_byte_count);
        if (cache->cache_data == NULL) {
            cache->sector_ix = 0xFFFF;
            cache->cache_size = 0;
            cache->cache_count = 0;
        } else {
            cache->cache_size = new_size;
        }
    }

    if (cache->cache_data) {
        cache->cache_data[entry_ix] = elem_size;
        cache->cache_count++;
    }
}

int
fcb_step(struct fcb *fcb, struct fcb_entry *loc, int previous_error)
{
    int offset;
    int entry_len;
    int rc = 0;
    int first_entry_offset = fcb_start_offset(fcb);

    if (FCB_STEP_BACK(loc)) {
        if (loc->fe_area == NULL) {
            /*
             * Stepping back with empty fcb_entry means that walks starts from
             * last entry in active sector.
             * If FCB is not empty f_active has inconsistent data: fe_data_len and fe_data_off
             * refer to last saved entry while fe_elem_off points past this entry where new
             * record should be written hence loc is is reconstructed.
             */
            if (fcb->f_active.fe_elem_off != first_entry_offset) {
                loc->fe_area = fcb->f_active.fe_area;
                loc->fe_elem_ix = fcb->f_active_sector_entry_count - 1;
                loc->fe_elem_off =
                    fcb->f_active.fe_data_off - fcb_len_in_flash(fcb, fcb_len_bytes(fcb->f_active.fe_data_len));
                loc->fe_data_off = fcb->f_active.fe_data_off;
                loc->fe_data_len = fcb->f_active.fe_data_len;
            } else {
                /* FCB is empty */
                rc = FCB_ERR_NOVAR;
            }
        } else {
            struct fcb_entry loc1 = *loc;
            int target_ix = loc->fe_elem_ix - 1;
            loc1.fe_data_len = 0;

            /* Begin of sector ? */
            if (loc->fe_elem_off == first_entry_offset) {
                /* Oldest sector, nowhere to go */
                if (fcb->f_oldest == loc->fe_area) {
                    rc = FCB_ERR_NOVAR;
                } else {
                    /* Switch to previous sector and find last entry */
                    loc1.fe_area = fcb_get_prev_area(fcb, loc->fe_area);
                    loc1.fe_elem_off = 0;
                    loc1.fe_elem_ix = 0;
                }
            }
            if (rc != FCB_ERR_NOVAR) {
                int data_len;
                struct fcb_entry_cache *cache = FCB_ENTRY_CACHE(loc);
                /* Empty cache if sector changed */
                fcb_cache_switch_to_sector(fcb, cache, fcb_get_sector_ix(fcb, loc1.fe_area));

                if (loc->fe_elem_off && target_ix >= 0 && (data_len = fcb_cache_elem_len(fcb, cache, target_ix)) > 0) {
                    loc->fe_elem_ix = target_ix;
                    loc->fe_data_len = data_len;
                    loc->fe_elem_off = loc->fe_elem_off - fcb_entry_total_len(fcb, data_len);
                    loc->fe_data_off = loc->fe_elem_off + fcb_len_in_flash(fcb, data_len);
                } else {
                    loc1.fe_elem_off = first_entry_offset;
                    loc1.fe_data_len = 0;
                    loc1.fe_elem_ix = 0;
                    if (cache && cache->cache_data) {
                        for (loc1.fe_elem_ix = 0;
                             loc1.fe_elem_ix < cache->cache_count && loc1.fe_elem_ix < target_ix;) {
                            loc1.fe_data_len = fcb_cache_elem_len(fcb, cache, loc1.fe_elem_ix);
                            loc1.fe_elem_off += fcb_entry_total_len(fcb, loc1.fe_data_len);
                            loc1.fe_elem_ix++;
                        }
                    }

                    entry_len = 0;
                    while (true) {
                        rc = fcb_update_entry_len(fcb, &loc1);
                        if (rc < 0) {
                            /*
                             * Here either there is no more entries or there is flash error.
                             * Get data from loc1 that was set from previous entry.
                             * If entry_len == 0 it means that not even single entry was found.
                             * It means that there was no entry in sector return rc without
                             * modifying loc.
                             */
                            if (entry_len) {
                                /* data_off and data_len are intact from previous read */
                                *loc = loc1;
                                /* fe_elem_off and fe_elem_ix were already moved, correct to last entry */
                                loc->fe_elem_off -= entry_len;
                                loc->fe_elem_ix--;
                                rc = 0;
                            }
                            break;
                        } else if (loc1.fe_elem_ix == target_ix) {
                            *loc = loc1;
                            break;
                        }
                        fcb_cache_add(fcb, cache, loc1.fe_elem_ix, loc1.fe_elem_off, loc1.fe_data_len);
                        entry_len = fcb_entry_len_in_flash(fcb, &loc1);
                        offset = (int)loc1.fe_elem_off + entry_len;
                        loc1.fe_elem_off = offset;
                        loc1.fe_elem_ix++;
                    }
                }
            }
        }
    } else {
        if (loc->fe_area == NULL) {
            loc->fe_area = fcb->f_oldest;
            loc->fe_elem_off = fcb_start_offset(fcb);
            loc->fe_elem_ix = 0;
            loc->fe_data_len = 0;
        } else if (previous_error == FCB_ERR_NOVAR) {
            /* If there are more sectors, move to next one */
            if (loc->fe_area != fcb->f_active.fe_area) {
                loc->fe_area = fcb_getnext_area(fcb, loc->fe_area);
                loc->fe_elem_off = fcb_start_offset(fcb);
                loc->fe_elem_ix = 0;
                loc->fe_data_len = 0;
            } else {
                /* No more sectors NOVAR sticks */
                rc = FCB_ERR_NOVAR;
            }
        } else if (loc->fe_elem_off == 0) {
            loc->fe_elem_off = fcb_start_offset(fcb);
            loc->fe_elem_ix = 0;
            loc->fe_data_len = 0;
        } else {
            loc->fe_elem_off += fcb_entry_len_in_flash(fcb, loc);
            loc->fe_elem_ix++;
        }
    }

    return rc;
}

int
fcb_getnext_nolock(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc = 0;

    do {
        rc = fcb_step(fcb, loc, rc);
        if (rc) {
            break;
        }
        rc = fcb_elem_info(fcb, loc);
    } while (rc == FCB_ERR_CRC || rc == FCB_ERR_NOVAR);

    return rc;
}

int
fcb_getnext(struct fcb *fcb, struct fcb_entry *loc)
{
    int rc;

    rc = os_mutex_pend(&fcb->f_mtx, OS_WAIT_FOREVER);
    if (rc && rc != OS_NOT_STARTED) {
        return FCB_ERR_ARGS;
    }
    rc = fcb_getnext_nolock(fcb, loc);
    os_mutex_release(&fcb->f_mtx);

    return rc;
}

int
fcb_cache_init(struct fcb *fcb, struct fcb_entry_cache *cache, int initial_entry_count)
{
    int rc;
    size_t cache_byte_count;

    (void)fcb;

    cache->sector_ix = 0xFFFF;
    cache->cache_count = 0;

    cache_byte_count = initial_entry_count * sizeof(cache->cache_data[0]);
    cache->cache_data = os_malloc(cache_byte_count);

    if (cache->cache_data) {
        cache->cache_size = initial_entry_count;
        rc = 0;
    } else {
        cache->cache_size = 0;
        rc = SYS_ENOMEM;
    }

    return rc;
}

void
fcb_cache_free(struct fcb *fcb, struct fcb_entry_cache *cache)
{
    (void)fcb;

    if (cache && cache->cache_data) {
        os_free(cache->cache_data);
        cache->cache_data = NULL;
        cache->cache_size = 0;
        cache->cache_count = 0;
        cache->sector_ix = 0xFFFF;
    }
}
