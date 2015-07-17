#include <assert.h>
#include <string.h>
#include "os/os_mempool.h"
#include "ffs/ffs.h"
#include "ffs_priv.h"

uint32_t
ffs_base_disk_size(const struct ffs_base *base)
{
    const struct ffs_inode *inode;
    const struct ffs_block *block;

    switch (base->fb_type) {
    case FFS_OBJECT_TYPE_INODE:
        inode = (void *)base;
        return sizeof (struct ffs_disk_inode) + inode->fi_filename_len;

    case FFS_OBJECT_TYPE_BLOCK:
        block = (void *)base;
        return sizeof (struct ffs_disk_block) + block->fb_data_len;

    default:
        assert(0);
        break;
    }
}

