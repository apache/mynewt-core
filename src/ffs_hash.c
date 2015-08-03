#include <stddef.h>
#include "ffs/ffs.h"
#include "ffs_priv.h"

struct ffs_object_list ffs_hash[FFS_HASH_SIZE];

static int
ffs_hash_fn(uint32_t id)
{
    return id % FFS_HASH_SIZE;
}

struct ffs_object *
ffs_hash_find(uint32_t id)
{
    struct ffs_object_list *list;
    struct ffs_object *object;
    int idx;

    idx = ffs_hash_fn(id);
    list = ffs_hash + idx;

    SLIST_FOREACH(object, list, fb_hash_next) {
        if (object->fo_id == id) {
            return object;
        }
    }

    return NULL;
}

int
ffs_hash_find_inode(struct ffs_inode **out_inode, uint32_t id)
{
    struct ffs_object *object;

    object = ffs_hash_find(id);
    if (object == NULL) {
        *out_inode = NULL;
        return FFS_ENOENT;
    }

    if (object->fo_type != FFS_OBJECT_TYPE_INODE) {
        *out_inode = NULL;
        return FFS_EUNEXP;
    }

    *out_inode = (void *)object;
    return 0;
}

int
ffs_hash_find_block(struct ffs_block **out_block, uint32_t id)
{
    struct ffs_object *object;

    object = ffs_hash_find(id);
    if (object == NULL) {
        *out_block = NULL;
        return FFS_ENOENT;
    }

    if (object->fo_type != FFS_OBJECT_TYPE_BLOCK) {
        *out_block = NULL;
        return FFS_EUNEXP;
    }

    *out_block = (void *)object;
    return 0;
}

void
ffs_hash_insert(struct ffs_object *object)
{
    struct ffs_object_list *list;
    int idx;

    idx = ffs_hash_fn(object->fo_id);
    list = ffs_hash + idx;

    SLIST_NEXT(object, fb_hash_next) = SLIST_FIRST(list);
    SLIST_FIRST(list) = object;
}

void
ffs_hash_remove(struct ffs_object *object)
{
    struct ffs_object_list *list;
    int idx;

    idx = ffs_hash_fn(object->fo_id);
    list = ffs_hash + idx;

    SLIST_REMOVE(list, object, ffs_object, fb_hash_next);
}

void
ffs_hash_init(void)
{
    int i;

    for (i = 0; i < FFS_HASH_SIZE; i++) {
        ffs_hash[i] =
            (struct ffs_object_list)SLIST_HEAD_INITIALIZER(&ffs_hash[i]);
    }
}

