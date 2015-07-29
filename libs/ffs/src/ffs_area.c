#include <assert.h>
#include <string.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"

int
ffs_area_desc_validate(const struct ffs_area_desc *area_desc)
{
    return 0;
}

void
ffs_area_set_magic(struct ffs_disk_area *disk_area)
{
    disk_area->fds_magic[0] = FFS_AREA_MAGIC0;
    disk_area->fds_magic[1] = FFS_AREA_MAGIC1;
    disk_area->fds_magic[2] = FFS_AREA_MAGIC2;
    disk_area->fds_magic[3] = FFS_AREA_MAGIC3;
}

int
ffs_area_magic_is_set(const struct ffs_disk_area *disk_area)
{
    return disk_area->fds_magic[0] == FFS_AREA_MAGIC0 &&
           disk_area->fds_magic[1] == FFS_AREA_MAGIC1 &&
           disk_area->fds_magic[2] == FFS_AREA_MAGIC2 &&
           disk_area->fds_magic[3] == FFS_AREA_MAGIC3;
}

int
ffs_area_is_scratch(const struct ffs_disk_area *disk_area)
{
    return ffs_area_magic_is_set(disk_area) &&
           disk_area->fds_is_scratch == 0xff;
}

void
ffs_area_to_disk(struct ffs_disk_area *out_disk_area,
                   const struct ffs_area *area)
{
    memset(out_disk_area, 0, sizeof *out_disk_area);
    ffs_area_set_magic(out_disk_area);
    out_disk_area->fds_length = area->fs_length;
    out_disk_area->fds_seq = area->fs_seq;
}

uint32_t
ffs_area_free_space(const struct ffs_area *area)
{
    return area->fs_length - area->fs_cur;
}

