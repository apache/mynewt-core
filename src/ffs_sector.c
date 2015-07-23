#include <assert.h>
#include <string.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"

int
ffs_sector_desc_validate(const struct ffs_sector_desc *sector_desc)
{
    return 0;
}

void
ffs_sector_set_magic(struct ffs_disk_sector *disk_sector)
{
    disk_sector->fds_magic[0] = FFS_SECTOR_MAGIC0;
    disk_sector->fds_magic[1] = FFS_SECTOR_MAGIC1;
    disk_sector->fds_magic[2] = FFS_SECTOR_MAGIC2;
    disk_sector->fds_magic[3] = FFS_SECTOR_MAGIC3;
}

int
ffs_sector_magic_is_set(const struct ffs_disk_sector *disk_sector)
{
    return disk_sector->fds_magic[0] == FFS_SECTOR_MAGIC0 &&
           disk_sector->fds_magic[1] == FFS_SECTOR_MAGIC1 &&
           disk_sector->fds_magic[2] == FFS_SECTOR_MAGIC2 &&
           disk_sector->fds_magic[3] == FFS_SECTOR_MAGIC3;
}

int
ffs_sector_is_scratch(const struct ffs_disk_sector *disk_sector)
{
    return ffs_sector_magic_is_set(disk_sector) &&
           disk_sector->fds_is_scratch == 0xff;
}

void
ffs_sector_to_disk(struct ffs_disk_sector *out_disk_sector,
                   const struct ffs_sector *sector)
{
    memset(out_disk_sector, 0, sizeof *out_disk_sector);
    ffs_sector_set_magic(out_disk_sector);
    out_disk_sector->fds_length = sector->fs_length;
    out_disk_sector->fds_seq = sector->fs_seq;
}

uint32_t
ffs_sector_free_space(const struct ffs_sector *sector)
{
    return sector->fs_length - sector->fs_cur;
}

