/**
 * Copyright (c) 2015 Runtime Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "ffs_priv.h"
#include "crc16.h"

int
ffs_crc_flash(uint16_t initial_crc, uint8_t area_idx, uint32_t area_offset,
              uint32_t len, uint16_t *out_crc)
{
    uint32_t chunk_len;
    uint16_t crc;
    int rc;

    crc = initial_crc;

    /* Copy data in chunks small enough to fit in the flash buffer. */
    while (len > 0) {
        if (len > sizeof ffs_flash_buf) {
            chunk_len = sizeof ffs_flash_buf;
        } else {
            chunk_len = len;
        }

        rc = ffs_flash_read(area_idx, area_offset, ffs_flash_buf, chunk_len);
        if (rc != 0) {
            return rc;
        }

        crc = crc16_ccitt(crc, ffs_flash_buf, chunk_len);

        area_offset += chunk_len;
        len -= chunk_len;
    }

    *out_crc = crc;
    return 0;
}

uint16_t
ffs_crc_disk_block_hdr(const struct ffs_disk_block *disk_block)
{
    uint16_t crc;

    crc = crc16_ccitt(0, disk_block, FFS_DISK_BLOCK_OFFSET_CRC);

    return crc;
}

static int
ffs_crc_disk_block(const struct ffs_disk_block *disk_block,
                   uint8_t area_idx, uint32_t area_offset,
                   uint16_t *out_crc)
{
    uint16_t crc;
    int rc;

    crc = ffs_crc_disk_block_hdr(disk_block);

    rc = ffs_crc_flash(crc, area_idx, area_offset + sizeof *disk_block,
                       disk_block->fdb_data_len, &crc);
    if (rc != 0) {
        return rc;
    }

    *out_crc = crc;
    return 0;
}

int
ffs_crc_disk_block_validate(const struct ffs_disk_block *disk_block,
                            uint8_t area_idx, uint32_t area_offset)
{
    uint16_t crc;
    int rc;

    rc = ffs_crc_disk_block(disk_block, area_idx, area_offset, &crc);
    if (rc != 0) {
        return rc;
    }

    if (crc != disk_block->fdb_crc16) {
        return FFS_ECORRUPT;
    }

    return 0;
}

void
ffs_crc_disk_block_fill(struct ffs_disk_block *disk_block, const void *data)
{
    uint16_t crc16;

    crc16 = ffs_crc_disk_block_hdr(disk_block);
    crc16 = crc16_ccitt(crc16, data, disk_block->fdb_data_len);

    disk_block->fdb_crc16 = crc16;
}

static uint16_t
ffs_crc_disk_inode_hdr(const struct ffs_disk_inode *disk_inode)
{
    uint16_t crc;

    crc = crc16_ccitt(0, disk_inode, FFS_DISK_INODE_OFFSET_CRC);

    return crc;
}

static int
ffs_crc_disk_inode(const struct ffs_disk_inode *disk_inode,
                     uint8_t area_idx, uint32_t area_offset,
                     uint16_t *out_crc)
{
    uint16_t crc;
    int rc;

    crc = ffs_crc_disk_inode_hdr(disk_inode);

    rc = ffs_crc_flash(crc, area_idx, area_offset + sizeof *disk_inode,
                       disk_inode->fdi_filename_len, &crc);
    if (rc != 0) {
        return rc;
    }

    *out_crc = crc;
    return 0;
}

int
ffs_crc_disk_inode_validate(const struct ffs_disk_inode *disk_inode,
                            uint8_t area_idx, uint32_t area_offset)
{
    uint16_t crc;
    int rc;

    rc = ffs_crc_disk_inode(disk_inode, area_idx, area_offset, &crc);
    if (rc != 0) {
        return rc;
    }

    if (crc != disk_inode->fdi_crc16) {
        return FFS_ECORRUPT;
    }

    return 0;
}

void
ffs_crc_disk_inode_fill(struct ffs_disk_inode *disk_inode,
                        const char *filename)
{
    uint16_t crc16;

    crc16 = ffs_crc_disk_inode_hdr(disk_inode);
    crc16 = crc16_ccitt(crc16, filename, disk_inode->fdi_filename_len);

    disk_inode->fdi_crc16 = crc16;
}
