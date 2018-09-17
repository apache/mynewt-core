/* Licensed to the Apache Software Foundation (ASF) under one
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
    struct sector_range *range;
    struct sector_range *newest_srp = NULL;
    int rc;
    int i;
    int max_align = 1;
    int align;
    int oldest = -1, newest = -1;
    int oldest_sec = -1, newest_sec = -1;
    struct fcb_disk_area fda;

    if (!fcb->f_ranges || fcb->f_sector_cnt - fcb->f_scratch_cnt < 1) {
        return FCB_ERR_ARGS;
    }

    /* Fill last used, first used */
    for (i = 0; i < fcb->f_sector_cnt; i++) {
        range = fcb_get_sector_range(fcb, i);
        align = flash_area_align(&range->sr_flash_area);
        if (align > max_align) {
            max_align = align;
        }
        rc = fcb_sector_hdr_read(fcb, range, i, &fda);
        if (rc < 0) {
            return rc;
        }
        if (rc == 0) {
            continue;
        }
        if (oldest < 0) {
            oldest = newest = fda.fd_id;
            oldest_sec = newest_sec = i;
            newest_srp = range;
            continue;
        }
        if (FCB_ID_GT(fda.fd_id, newest)) {
            newest = fda.fd_id;
            newest_sec = i;
            newest_srp = range;
        } else if (FCB_ID_GT(oldest, fda.fd_id)) {
            oldest = fda.fd_id;
            oldest_sec = i;
        }
    }
    if (oldest < 0) {
        /*
         * No initialized areas.
         */
        oldest_sec = newest_sec = 0;
        newest_srp = fcb->f_ranges;
        rc = fcb_sector_hdr_init(fcb, newest_sec, 0);
        if (rc) {
            return rc;
        }
        newest = oldest = 0;
    }
    fcb->f_align = max_align;
    fcb->f_oldest_sec = oldest_sec;
    fcb->f_active.fe_range = newest_srp;
    fcb->f_active.fe_sector = newest_sec;
    fcb->f_active.fe_elem_off = sizeof(struct fcb_disk_area);
    fcb->f_active_id = newest;

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
    os_mutex_init(&fcb->f_mtx);
    return rc;
}

int
fcb_free_sector_cnt(struct fcb *fcb)
{
    int i;
    int sector;

    sector = fcb->f_active.fe_sector;
    for (i = 0; i < fcb->f_sector_cnt; i++) {
        sector = fcb_getnext_sector(fcb, sector);
        if (sector == fcb->f_oldest_sec) {
            break;
        }
    }
    return i;
}

int
fcb_is_empty(struct fcb *fcb)
{
    return (fcb->f_active.fe_sector == fcb->f_oldest_sec &&
      fcb->f_active.fe_elem_off == sizeof(struct fcb_disk_area));
}

struct sector_range *
fcb_get_sector_range(const struct fcb *fcb, int sector)
{
    int i;
    struct sector_range *srp = fcb->f_ranges;

    if (FCB_SECTOR_OLDEST == sector) {
        sector = fcb->f_oldest_sec;
    }
    for (i = 0; i < fcb->f_range_cnt; ++i, ++srp) {
        if (srp->sr_sector_count <= sector) {
            sector -= srp->sr_sector_count;
            continue;
        }
        return srp;
    }
    return NULL;
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
fcb_sector_hdr_init(struct fcb *fcb, int sector, uint16_t id)
{
    struct fcb_disk_area fda;
    struct fcb_sector_info info;
    struct sector_range *range;
    int sector_in_range;
    int rc;

    rc = fcb_get_sector_info(fcb, sector, &info);
    if (rc) {
        return rc;
    }
    range = info.si_range;
    sector_in_range = sector - range->sr_first_sector;

    fda.fd_magic = fcb->f_magic;
    fda.fd_ver = fcb->f_version;
    fda._pad = 0xff;
    fda.fd_id = id;

    assert(sector_in_range >= 0 && sector_in_range < range->sr_sector_count);
    rc = flash_area_write(&range->sr_flash_area,
        sector_in_range * range->sr_sector_size, &fda, sizeof(fda));
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
fcb_sector_hdr_read(struct fcb *fcb, struct sector_range *srp, uint16_t sec,
  struct fcb_disk_area *fdap)
{
    struct fcb_disk_area fda;
    int rc;
    uint32_t off = (sec - srp->sr_first_sector) * srp->sr_sector_size;

    if (!fdap) {
        fdap = &fda;
    }
    rc = flash_area_read_is_empty(&srp->sr_flash_area, off, fdap, sizeof(*fdap));
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
 * @return 0 on there are any fcbs aviable; OS_ENOENT otherwise
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

    return (i == 0) ? OS_ENOENT : 0;
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

int
fcb_init_flash_area(struct fcb *fcb, int flash_area_id, uint8_t version)
{
    const struct flash_area *fa;
    struct sector_range *sector_ranges;
    int sector_range_cnt = 0;
    int rc;

    /*
     * We don't know how big the area is so need to check how many sectors are
     * there and then read information about all sectors - this is needed to
     * properly initialize FCB. Critical log is somewhat important and shall be
     * created, so we just assert on any error.
     *
     * XXX Should we do something else here?
     */
    rc = flash_area_to_sector_ranges(flash_area_id, &sector_range_cnt, NULL);
    assert(rc == 0 && sector_range_cnt > 0);
    sector_ranges = malloc(sizeof(struct sector_range) * sector_range_cnt);
    assert(sector_ranges);
    rc = flash_area_to_sector_ranges(flash_area_id, &sector_range_cnt, sector_ranges);
    assert(rc == 0 && sector_range_cnt > 0);

    fcb->f_ranges = sector_ranges;
    fcb->f_range_cnt = sector_range_cnt;
    fcb->f_sector_cnt = sector_ranges[sector_range_cnt - 1].sr_first_sector +
        sector_ranges[sector_range_cnt - 1].sr_sector_count;
    fcb->f_magic = 0xDEADBEEF;
    fcb->f_version = version;

    /*
     * Initialize log in dedicated flash area. This has to succeed since it
     * should be in dedicated flash area and nothing should prevent us from
     * creating log there.
     */
    rc = fcb_init(fcb);
    if (rc) {
        /* Need to erase full area here */
        rc = flash_area_open(flash_area_id, &fa);
        assert(rc == 0);

        flash_area_erase(fa, 0, fa->fa_size);
        rc = fcb_init(fcb);
        assert(rc == 0);
    }

    return rc;
}

int
fcb_get_sector_info(const struct fcb *fcb, int sector,
    struct fcb_sector_info *info)
{
    struct sector_range *srp = fcb->f_ranges;
    int i;

    if (sector == FCB_SECTOR_OLDEST) {
        sector = fcb->f_oldest_sec;
    }

    for (i = 0; i < fcb->f_range_cnt; ++i, ++srp) {
        if (srp->sr_sector_count <= sector) {
            sector -= srp->sr_sector_count;
            continue;
        }
        info->si_range = srp;
        info->si_sector_in_range = sector;
        info->si_sector_offset = srp->sr_range_start +
            sector * srp->sr_sector_size;
        return 0;
    }
    return FCB_ERR_ARGS;
}

int
fcb_get_sector_loc(const struct fcb *fcb, int sector, struct fcb_entry *entry)
{
    struct fcb_sector_info info;
    int rc = 0;

    if (sector == FCB_SECTOR_OLDEST) {
        sector = fcb->f_oldest_sec;
    }
    rc = fcb_get_sector_info(fcb, sector, &info);
    if (rc == 0) {
        entry->fe_range = info.si_range;
        entry->fe_sector = (uint16_t)sector;
    }

    return rc;
}


int
fcb_offset_in_sector(const struct fcb *fcb, int offset, int sector)
{
    struct sector_range *srp = fcb->f_ranges;
    int start;
    int end;
    int i;

    for (i = 0; i < fcb->f_range_cnt; ++i, ++srp) {
        if (srp->sr_sector_count * srp->sr_sector_size > offset) {
            start = (sector - srp->sr_first_sector) * srp->sr_sector_size;
            end = start + srp->sr_sector_size;
            if (start <= offset && offset < end)
                return 0;
        } else {
            offset -= srp->sr_sector_count * srp->sr_sector_size;
        }
    }
    return -1;
}

int
fcb_get_sector_offset(const struct fcb *fcb, int sector)
{
    struct fcb_sector_info info;
    int rc = fcb_get_sector_info(fcb, sector, &info);

    if (rc == 0) {
        rc = info.si_sector_offset;
    }
    return rc;
}

int
fcb_entry_get_sector_offset(const struct fcb_entry *loc)
{
    const struct sector_range *srp = loc->fe_range;

    return srp->sr_range_start +
        (loc->fe_sector - srp->sr_first_sector) * srp->sr_sector_size;
}

int
fcb_write_to_sector(struct fcb_entry *loc, int off, const void *buf, int len)
{
    return flash_area_write(&loc->fe_range->sr_flash_area,
        fcb_sector_flash_offset(loc) + off, buf, len);
}

int
fcb_read_from_sector(struct fcb_entry *loc, int off, void *buf, int len)
{
    return flash_area_read(&loc->fe_range->sr_flash_area,
        fcb_sector_flash_offset(loc) + off, buf, len);
}

int
fcb_get_total_size(const struct fcb *fcb)
{
    struct sector_range *srp = fcb->f_ranges;
    int size = 0;
    int i;

    for (i = 0; i < fcb->f_range_cnt; ++i, ++srp) {
        size += srp->sr_sector_count * srp->sr_sector_count;
    }
    return size;
}

int
fcb_sector_erase(const struct fcb *fcb, int sector)
{
    struct fcb_sector_info info;
    int rc;

    rc = fcb_get_sector_info(fcb, sector, &info);
    if (rc) {
        goto end;
    }

    rc = flash_area_erase(&info.si_range->sr_flash_area,
        info.si_sector_in_range * info.si_range->sr_sector_size,
        info.si_range->sr_sector_size);
end:
    return rc;
}
