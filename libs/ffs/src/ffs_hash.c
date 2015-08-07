#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "ffs/ffs.h"
#include "ffs_priv.h"

struct ffs_hash_list ffs_hash[FFS_HASH_SIZE];

uint32_t ffs_hash_next_dir_id;
uint32_t ffs_hash_next_file_id;
uint32_t ffs_hash_next_block_id;

int
ffs_hash_id_is_dir(uint32_t id)
{
    return id >= FFS_ID_DIR_MIN && id < FFS_ID_DIR_MAX;
}

int
ffs_hash_id_is_file(uint32_t id)
{
    return id >= FFS_ID_FILE_MIN && id < FFS_ID_FILE_MAX;
}

int
ffs_hash_id_is_inode(uint32_t id)
{
    return ffs_hash_id_is_dir(id) || ffs_hash_id_is_file(id);
}

int
ffs_hash_id_is_block(uint32_t id)
{
    return id >= FFS_ID_BLOCK_MIN && id < FFS_ID_BLOCK_MAX;
}

static int
ffs_hash_fn(uint32_t id)
{
    return id % FFS_HASH_SIZE;
}

struct ffs_hash_entry *
ffs_hash_find(uint32_t id)
{
    struct ffs_hash_entry *entry;
    struct ffs_hash_list *list;
    int idx;

    idx = ffs_hash_fn(id);
    list = ffs_hash + idx;

    SLIST_FOREACH(entry, list, fhe_next) {
        if (entry->fhe_id == id) {
            return entry;
        }
    }

    return NULL;
}

struct ffs_inode_entry *
ffs_hash_find_inode(uint32_t id)
{
    struct ffs_hash_entry *entry;

    assert(ffs_hash_id_is_inode(id));

    entry = ffs_hash_find(id);
    return (struct ffs_inode_entry *)entry;
}

struct ffs_hash_entry *
ffs_hash_find_block(uint32_t id)
{
    struct ffs_hash_entry *entry;

    assert(ffs_hash_id_is_block(id));

    entry = ffs_hash_find(id);
    return entry;
}

void
ffs_hash_insert(struct ffs_hash_entry *entry)
{
    struct ffs_hash_list *list;
    int idx;

    idx = ffs_hash_fn(entry->fhe_id);
    list = ffs_hash + idx;

    SLIST_NEXT(entry, fhe_next) = SLIST_FIRST(list);
    SLIST_FIRST(list) = entry;
}

void
ffs_hash_remove(struct ffs_hash_entry *entry)
{
    struct ffs_hash_list *list;
    int idx;

    idx = ffs_hash_fn(entry->fhe_id);
    list = ffs_hash + idx;

    SLIST_REMOVE(list, entry, ffs_hash_entry, fhe_next);
}

struct ffs_hash_entry *
ffs_hash_entry_alloc(void)
{
    struct ffs_hash_entry *entry;

    entry = os_memblock_get(&ffs_hash_entry_pool);
    if (entry != NULL) {
        memset(entry, 0, sizeof *entry);
    }

    return entry;
}

void
ffs_hash_entry_free(struct ffs_hash_entry *entry)
{
    os_memblock_put(&ffs_hash_entry_pool, entry);
}


void
ffs_hash_init(void)
{
    int i;

    for (i = 0; i < FFS_HASH_SIZE; i++) {
        ffs_hash[i] =
            (struct ffs_hash_list)SLIST_HEAD_INITIALIZER(&ffs_hash[i]);
    }
}

