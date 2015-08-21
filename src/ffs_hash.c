#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "ffs/ffs.h"
#include "ffs_priv.h"

struct ffs_hash_list *ffs_hash;

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
    return id % ffs_config.fc_hash_size;
}

struct ffs_hash_entry *
ffs_hash_find(uint32_t id)
{
    struct ffs_hash_entry *entry;
    struct ffs_hash_entry *prev;
    struct ffs_hash_list *list;
    int idx;

    idx = ffs_hash_fn(id);
    list = ffs_hash + idx;

    prev = NULL;
    SLIST_FOREACH(entry, list, fhe_next) {
        if (entry->fhe_id == id) {
            /* Put entry at the front of the list. */
            if (prev != NULL) {
                SLIST_NEXT(prev, fhe_next) = SLIST_NEXT(entry, fhe_next);
                SLIST_INSERT_HEAD(list, entry, fhe_next);
            }
            return entry;
        }

        prev = entry;
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

    SLIST_INSERT_HEAD(list, entry, fhe_next);
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

int
ffs_hash_init(void)
{
    int i;

    free(ffs_hash);

    ffs_hash = malloc(ffs_config.fc_hash_size * sizeof *ffs_hash);
    if (ffs_hash == NULL) {
        return FFS_ENOMEM;
    }

    for (i = 0; i < ffs_config.fc_hash_size; i++) {
        SLIST_INIT(ffs_hash + i);
    }

    return 0;
}

