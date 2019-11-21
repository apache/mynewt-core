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
#ifndef __SYS_FCB2_PRIV_H_
#define __SYS_FCB2_PRIV_H_

#ifdef __cplusplus
extern "C" {
#endif

#define FCB2_TMP_BUF_SZ  32

#define FCB2_ID_GT(a, b) (((int16_t)(a) - (int16_t)(b)) > 0)

struct fcb2_disk_area {
    uint32_t fd_magic;
    uint8_t  fd_ver;
    uint8_t  _pad;
    uint16_t fd_id;
};

struct fcb2_sector_info {
    struct flash_sector_range *si_range;  /* Sector range */
    uint32_t si_sector_offset;            /* Sector offset in fcb */
    uint16_t si_sector_in_range;          /* Sector number relative to si_range */
};

static inline int
fcb2_len_in_flash(const struct flash_sector_range *range, uint16_t len)
{
    if (range->fsr_align <= 1) {
        return len;
    }
    return (len + (range->fsr_align - 1)) & ~(range->fsr_align - 1);
}

int fcb2_getnext_in_area(struct fcb2 *fcb, struct fcb2_entry *loc);

static inline int
fcb2_getnext_sector(struct fcb2 *fcb, int sector)
{
    if (++sector >= fcb->f_sector_cnt) {
        sector = 0;
    }
    return sector;
}

int fcb2_getnext_nolock(struct fcb2 *fcb, struct fcb2_entry *loc);

int fcb2_elem_info(struct fcb2_entry *loc);
int fcb2_elem_crc16(struct fcb2_entry *loc, uint16_t *c16p);
int fcb2_sector_hdr_init(struct fcb2 *fcb, int sector, uint16_t id);
int fcb2_entry_location_in_range(const struct fcb2_entry *loc);

struct flash_sector_range *fcb2_get_sector_range(const struct fcb2 *fcb,
                                                 int sector);

int fcb2_sector_hdr_read(struct fcb2 *, struct flash_sector_range *srp,
                         uint16_t sec, struct fcb2_disk_area *fdap);

/**
 * Finds sector range for given fcb sector.
 */
struct flash_sector_range *fcb2_get_sector_range(const struct fcb2 *fcb,
                                                 int sector);

/**
 * @brief Get information about sector from fcb.
 *
 * @param fcb     fcb to check sector
 * @param sector  fcb sector number
 *                sector can be specified as FCB2_SECTOR_OLDEST.
 * @param info    pointer to structure that will receive sector information
 *
 * @return 0 if sector information was extracted correctly
 *        FCB2_ERR_ARGS if sector number was outside fcb range.
 */
int fcb2_get_sector_info(const struct fcb2 *fcb, int sector,
                         struct fcb2_sector_info *info);

/**
 * @brief Write data to fcb sector.
 *
 * @param loc     location of the sector from fcb2_get_sector_loc().
 * @param off     offset from the beginning of the sector to start write
 * @param buf     pointer to data to be written
 * @param len     number of bytes to write to the fcb sector
 *
 * @return 0 if write was successful non zero otherwise.
 */
int fcb2_write_to_sector(struct fcb2_entry *loc, int off,
                         const void *buf, int len);

/**
 * @brief Read data from fcb sector.
 *
 * @param loc     location of the sector from fcb2_get_sector_loc().
 * @param off     offset from the beginning of the sector to start read
 * @param buf     pointer to output buffer
 * @param len     number of bytes to read from the fcb sector
 *
 * @return 0 if read was successful non zero otherwise.
 */
int fcb2_read_from_sector(struct fcb2_entry *loc, int off, void *buf, int len);

#ifdef __cplusplus
}
#endif

#endif
