#include <stddef.h>
#include "ffs/ffs.h"
#include "ffs_priv.h"

struct ffs_base_list ffs_hash[FFS_HASH_SIZE];

static int
ffs_hash_fn(uint32_t id)
{
    return id % FFS_HASH_SIZE;
}

struct ffs_base *
ffs_hash_find(uint32_t id)
{
    struct ffs_base_list *list;
    struct ffs_base *base;
    int idx;

    idx = ffs_hash_fn(id);
    list = ffs_hash + idx;

    SLIST_FOREACH(base, list, fb_hash_next) {
        if (base->fb_id == id) {
            return base;
        }
    }

    return NULL;
}

int
ffs_hash_find_inode(struct ffs_inode **out_inode, uint32_t id)
{
    struct ffs_base *base;

    base = ffs_hash_find(id);
    if (base == NULL) {
        *out_inode = NULL;
        return FFS_ENOENT;
    }

    if (base->fb_type != FFS_OBJECT_TYPE_INODE) {
        *out_inode = NULL;
        return FFS_EUNEXP;
    }

    *out_inode = (void *)base;
    return 0;
}

int
ffs_hash_find_block(struct ffs_block **out_block, uint32_t id)
{
    struct ffs_base *base;

    base = ffs_hash_find(id);
    if (base == NULL) {
        *out_block = NULL;
        return FFS_ENOENT;
    }

    if (base->fb_type != FFS_OBJECT_TYPE_BLOCK) {
        *out_block = NULL;
        return FFS_EUNEXP;
    }

    *out_block = (void *)base;
    return 0;
}

void
ffs_hash_insert(struct ffs_base *base)
{
    struct ffs_base_list *list;
    int idx;

    idx = ffs_hash_fn(base->fb_id);
    list = ffs_hash + idx;

    SLIST_NEXT(base, fb_hash_next) = SLIST_FIRST(list);
    SLIST_FIRST(list) = base;
}

void
ffs_hash_remove(struct ffs_base *base)
{
    struct ffs_base_list *list;
    int idx;

    idx = ffs_hash_fn(base->fb_id);
    list = ffs_hash + idx;

    SLIST_REMOVE(list, base, ffs_base, fb_hash_next);
}

