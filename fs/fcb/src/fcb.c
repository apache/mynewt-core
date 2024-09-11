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
#include <limits.h>
#include <stdlib.h>

#include "fcb/fcb.h"
#include "fcb_priv.h"
#include "string.h"

int
fcb_init(struct fcb *fcb)
{
    struct flash_area *fap;
    int rc;
    int i;
    int max_align = 1;
    int align;
    int oldest = -1, newest = -1;
    struct flash_area *oldest_fap = NULL, *newest_fap = NULL;
    struct fcb_disk_area fda;

    if (!fcb->f_sectors || fcb->f_sector_cnt - fcb->f_scratch_cnt < 1) {
        return FCB_ERR_ARGS;
    }

    /* Fill last used, first used */
    for (i = 0; i < fcb->f_sector_cnt; i++) {
        fap = &fcb->f_sectors[i];
        align = flash_area_align(fap);
        if (align > max_align) {
            max_align = flash_area_align(fap);
        }
        rc = fcb_sector_hdr_read(fcb, fap, &fda);
        if (rc < 0) {
            return rc;
        }
        if (rc == 0) {
            continue;
        }
        if (oldest < 0) {
            oldest = newest = fda.fd_id;
            oldest_fap = newest_fap = fap;
            continue;
        }
        if (FCB_ID_GT(fda.fd_id, newest)) {
            newest = fda.fd_id;
            newest_fap = fap;
        } else if (FCB_ID_GT(oldest, fda.fd_id)) {
            oldest = fda.fd_id;
            oldest_fap = fap;
        }
    }
    if (oldest < 0) {
        /*
         * No initialized areas.
         */
        oldest_fap = newest_fap = &fcb->f_sectors[0];
        rc = fcb_sector_hdr_init(fcb, oldest_fap, 0);
        if (rc) {
            return rc;
        }
        newest = oldest = 0;
    }
    fcb->f_align = max_align;
    fcb->f_oldest = oldest_fap;
    fcb->f_active.fe_area = newest_fap;
    fcb->f_active.fe_elem_off = fcb_len_in_flash(fcb, sizeof(struct fcb_disk_area));
    fcb->f_active_id = newest;
    fcb->f_active.fe_elem_ix = 0;

    /* Require alignment to be a power of two.  Some code depends on this
     * assumption.
     */
    assert((fcb->f_align & (fcb->f_align - 1)) == 0);

    while (1) {
        rc = fcb_getnext_in_area(fcb, &fcb->f_active);
        if (rc == FCB_ERR_NOVAR) {
            rc = FCB_OK;
            break;
        }
        if (rc != 0) {
            break;
        }
    }
    fcb->f_active_sector_entry_count = fcb->f_active.fe_elem_ix;

    os_mutex_init(&fcb->f_mtx);
    return rc;
}

int
fcb_free_sector_cnt(struct fcb *fcb)
{
    int i;
    struct flash_area *fa;

    fa = fcb->f_active.fe_area;
    for (i = 0; i < fcb->f_sector_cnt; i++) {
        fa = fcb_getnext_area(fcb, fa);
        if (fa == fcb->f_oldest) {
            break;
        }
    }
    return i;
}

int
fcb_is_empty(struct fcb *fcb)
{
    return (fcb->f_active.fe_area == fcb->f_oldest &&
      fcb->f_active.fe_elem_off == sizeof(struct fcb_disk_area));
}

/**
 * Length of an element is encoded in 1 or 2 bytes.
 * 1 byte for lengths < 128 bytes, and 2 bytes for < 16384.
 */
int
fcb_put_len(uint8_t *buf, uint16_t len)
{
    if (len < 0x80) {
        buf[0] = len;
        return 1;
    } else if (len < FCB_MAX_LEN) {
        buf[0] = (len & 0x7f) | 0x80;
        buf[1] = len >> 7;
        return 2;
    } else {
        return FCB_ERR_ARGS;
    }
}

int
fcb_get_len(uint8_t *buf, uint16_t *len)
{
    int rc;

    if (buf[0] & 0x80) {
        *len = (buf[0] & 0x7f) | (buf[1] << 7);
        rc = 2;
    } else {
        *len = buf[0];
        rc = 1;
    }
    return rc;
}

/**
 * Initialize erased sector for use.
 */
int
fcb_sector_hdr_init(struct fcb *fcb, struct flash_area *fap, uint16_t id)
{
    struct fcb_disk_area fda;
    int rc;

    fda.fd_magic = fcb->f_magic;
    fda.fd_ver = fcb->f_version;
    fda._pad = 0xff;
    fda.fd_id = id;

    rc = flash_area_write(fap, 0, &fda, sizeof(fda));
    if (rc) {
        return FCB_ERR_FLASH;
    }
    return 0;
}

/**
 * Checks whether FCB sector contains data or not.
 * Returns <0 in error.
 * Returns 0 if sector is unused;
 * Returns 1 if sector has data.
 */
int
fcb_sector_hdr_read(struct fcb *fcb, struct flash_area *fap,
  struct fcb_disk_area *fdap)
{
    struct fcb_disk_area fda;
    int rc;

    if (!fdap) {
        fdap = &fda;
    }
    rc = flash_area_read_is_empty(fap, 0, fdap, sizeof(*fdap));
    if (rc < 0) {
        return FCB_ERR_FLASH;
    } else if (rc == 1) {
        return 0;
    }
    if (fdap->fd_magic != fcb->f_magic) {
        return FCB_ERR_MAGIC;
    }
    if (fdap->fd_ver != fcb->f_version) {
        return FCB_ERR_VERSION;
    }
    return 1;
}

/**
 * Finds the fcb entry that gives back upto n entries at the end.
 * @param0 ptr to fcb
 * @param1 n number of fcb entries the user wants to get
 * @param2 ptr to the fcb_entry to be returned
 * @return 0 on there are any fcbs aviable; FCB_ERR_NOVAR otherwise
 */
int
fcb_offset_last_n(struct fcb *fcb, uint8_t entries,
        struct fcb_entry *last_n_entry)
{
    struct fcb_entry loc;
    int i;

    /* assure a minimum amount of entries */
    if (!entries) {
        entries = 1;
    }

    i = 0;
    memset(&loc, 0, sizeof(loc));
    while (!fcb_getnext(fcb, &loc)) {
        if (i == 0) {
            /* Start from the beginning of fcb entries */
            *last_n_entry = loc;
        } else if (i > (entries - 1)) {
            /* Update last_n_entry after n entries and keep updating */
            fcb_getnext(fcb, last_n_entry);
        }
        i++;
    }

    return (i == 0) ? FCB_ERR_NOVAR : 0;
}

/**
 * Clear fcb
 * @param fcb
 * @return 0 on success; non-zero on failure
 */
int
fcb_clear(struct fcb *fcb)
{
    int rc;

    rc = 0;
    while (!fcb_is_empty(fcb)) {
        rc = fcb_rotate(fcb);
        if (rc) {
            break;
        }
    }
    return rc;
}
