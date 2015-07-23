#include <assert.h>
#include <string.h>
#include "hal/hal_flash.h"
#include "ffs_priv.h"
#include "ffs/ffs.h"

int
ffs_format_from_scratch_sector(uint16_t sector_id)
{
    struct ffs_disk_sector disk_sector;
    int rc;

    assert(sector_id < ffs_num_sectors);
    rc = ffs_flash_read(sector_id, 0, &disk_sector, sizeof disk_sector);
    if (rc != 0) {
        return rc;
    }

    if (!ffs_sector_is_scratch(&disk_sector)) {
        rc = ffs_format_sector(sector_id, 0);
        if (rc != 0) {
            return rc;
        }
    } else {
        disk_sector.fds_is_scratch = 0;
        rc = ffs_flash_write(sector_id, FFS_SECTOR_OFFSET_IS_SCRATCH,
                             &disk_sector.fds_is_scratch,
                             sizeof disk_sector.fds_is_scratch);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

int
ffs_format_sector(uint16_t sector_id, int is_scratch)
{
    struct ffs_disk_sector disk_sector;
    struct ffs_sector *sector;
    uint32_t write_len;
    int rc;

    sector = ffs_sectors + sector_id;

    rc = flash_erase(sector->fs_offset, sector->fs_length);
    if (rc != 0) {
        return rc;
    }
    sector->fs_cur = 0;

    ffs_sector_to_disk(&disk_sector, sector);

    if (is_scratch) {
        write_len = sizeof disk_sector - 1;
    } else {
        write_len = sizeof disk_sector;
    }

    rc = ffs_flash_write(sector_id, 0, &disk_sector.fds_magic, write_len);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

void
ffs_format_ram(void)
{
    struct ffs_base_list *list;
    struct ffs_inode *inode;
    struct ffs_base *base;
    int i;

    for (i = 0; i < FFS_HASH_SIZE; i++) {
        list = ffs_hash + i;

        base = SLIST_FIRST(list);
        while (base != NULL) {
            if (base->fb_type == FFS_OBJECT_TYPE_INODE) {
                inode = (void *)base;
                while (inode->fi_refcnt > 0) {
                    ffs_inode_dec_refcnt(inode);
                }
                base = SLIST_FIRST(list);
            } else {
                base = SLIST_NEXT(base, fb_hash_next);
            }
        }
    }
}

int
ffs_format_full(const struct ffs_sector_desc *sector_descs)
{
    int rc;
    int i;

    /* Select largest sector to be the initial scratch sector. */
    ffs_scratch_sector_id = 0;
    for (i = 1; sector_descs[i].fsd_length != 0; i++) {
        if (sector_descs[i].fsd_length >
            sector_descs[ffs_scratch_sector_id].fsd_length) {

            ffs_scratch_sector_id = i;
        }
    }

    ffs_num_sectors = i;
    for (i = 0; i < ffs_num_sectors; i++) {
        ffs_sectors[i].fs_offset = sector_descs[i].fsd_offset;
        ffs_sectors[i].fs_length = sector_descs[i].fsd_length;
        ffs_sectors[i].fs_cur = 0;
        ffs_sectors[i].fs_seq = 0;

        rc = ffs_format_sector(i, i == ffs_scratch_sector_id);
        if (rc != 0) {
            goto err;
        }
    }

    rc = ffs_misc_validate_scratch();
    if (rc != 0) {
        goto err;
    }

    ffs_format_ram();
    ffs_next_id = 0;
    ffs_root_dir = NULL;

    /* Create root directory. */
    rc = ffs_file_new(&ffs_root_dir, NULL, "", 0, 1);
    if (rc != 0) {
        goto err;
    }

    rc = ffs_misc_validate_root();
    if (rc != 0) {
        goto err;
    }

    return 0;

err:
    ffs_scratch_sector_id = FFS_SECTOR_ID_SCRATCH;
    ffs_num_sectors = 0;
    return rc;
}

