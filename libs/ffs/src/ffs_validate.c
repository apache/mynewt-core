#include <assert.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"

int
ffs_validate_root(void)
{
    if (ffs_root_dir == NULL) {
        return FFS_ECORRUPT;
    }

    return 0;
}

int
ffs_validate_scratch(void)
{
    uint32_t scratch_len;
    int i;

    if (ffs_scratch_sector_id == FFS_SECTOR_ID_SCRATCH) {
        /* No scratch sector. */
        return FFS_ECORRUPT;
    }

    scratch_len = ffs_sectors[ffs_scratch_sector_id].fsi_length;
    for (i = 0; i < ffs_num_sectors; i++) {
        if (ffs_sectors[i].fsi_length > scratch_len) {
            return FFS_ECORRUPT;
        }
    }

    return 0;
}

