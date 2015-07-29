#include <assert.h>
#include <string.h>
#include "hal/hal_flash.h"
#include "ffs_priv.h"
#include "ffs/ffs.h"

int
ffs_format_from_scratch_area(uint16_t area_id)
{
    struct ffs_disk_area disk_area;
    int rc;

    assert(area_id < ffs_num_areas);
    rc = ffs_flash_read(area_id, 0, &disk_area, sizeof disk_area);
    if (rc != 0) {
        return rc;
    }

    if (!ffs_area_is_scratch(&disk_area)) {
        rc = ffs_format_area(area_id, 0);
        if (rc != 0) {
            return rc;
        }
    } else {
        disk_area.fds_is_scratch = 0;
        rc = ffs_flash_write(area_id, FFS_AREA_OFFSET_IS_SCRATCH,
                             &disk_area.fds_is_scratch,
                             sizeof disk_area.fds_is_scratch);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

int
ffs_format_area(uint16_t area_id, int is_scratch)
{
    struct ffs_disk_area disk_area;
    struct ffs_area *area;
    uint32_t write_len;
    int rc;

    area = ffs_areas + area_id;

    rc = flash_erase(area->fs_offset, area->fs_length);
    if (rc != 0) {
        return rc;
    }
    area->fs_cur = 0;

    ffs_area_to_disk(&disk_area, area);

    if (is_scratch) {
        write_len = sizeof disk_area - 1;
    } else {
        write_len = sizeof disk_area;
    }

    rc = ffs_flash_write(area_id, 0, &disk_area.fds_magic, write_len);
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
ffs_format_full(const struct ffs_area_desc *area_descs)
{
    int rc;
    int i;

    /* Select largest area to be the initial scratch area. */
    ffs_scratch_area_id = 0;
    for (i = 1; area_descs[i].fad_length != 0; i++) {
        if (area_descs[i].fad_length >
            area_descs[ffs_scratch_area_id].fad_length) {

            ffs_scratch_area_id = i;
        }
    }

    ffs_num_areas = i;
    for (i = 0; i < ffs_num_areas; i++) {
        ffs_areas[i].fs_offset = area_descs[i].fad_offset;
        ffs_areas[i].fs_length = area_descs[i].fad_length;
        ffs_areas[i].fs_cur = 0;
        ffs_areas[i].fs_seq = 0;

        rc = ffs_format_area(i, i == ffs_scratch_area_id);
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
    ffs_scratch_area_id = FFS_AREA_ID_SCRATCH;
    ffs_num_areas = 0;
    return rc;
}

