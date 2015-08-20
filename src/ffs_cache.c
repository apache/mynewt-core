#include <assert.h>
#include <string.h>
#include "ffs/ffs.h"
#include "ffs_priv.h"

static void ffs_cache_block_free(struct ffs_cache_block *entry);

TAILQ_HEAD(ffs_cache_inode_list, ffs_cache_inode);
static struct ffs_cache_inode_list ffs_cache_inode_list =
    TAILQ_HEAD_INITIALIZER(ffs_cache_inode_list);

static void
ffs_cache_inode_free_blocks(struct ffs_cache_inode *cache_inode)
{
    struct ffs_cache_block *cache_block;

    while ((cache_block = TAILQ_FIRST(&cache_inode->fci_block_list)) != NULL) {
        TAILQ_REMOVE(&cache_inode->fci_block_list, cache_block, fcb_link);
        ffs_cache_block_free(cache_block);
    }
}

static struct ffs_cache_inode *
ffs_cache_inode_alloc(void)
{
    struct ffs_cache_inode *entry;

    entry = os_memblock_get(&ffs_cache_inode_pool);
    if (entry != NULL) {
        memset(entry, 0, sizeof *entry);
        entry->fci_block_list = (struct ffs_cache_block_list)
            TAILQ_HEAD_INITIALIZER(entry->fci_block_list);
    }

    return entry;
}

static void
ffs_cache_inode_free(struct ffs_cache_inode *entry)
{
    if (entry != NULL) {
        ffs_cache_inode_free_blocks(entry);
        os_memblock_put(&ffs_cache_inode_pool, entry);
    }
}

static struct ffs_cache_inode *
ffs_cache_inode_acquire(void)
{
    struct ffs_cache_inode *entry;

    entry = ffs_cache_inode_alloc();
    if (entry == NULL) {
        entry = TAILQ_LAST(&ffs_cache_inode_list, ffs_cache_inode_list);
        assert(entry != NULL);

        TAILQ_REMOVE(&ffs_cache_inode_list, entry, fci_link);
        ffs_cache_inode_free(entry);

        entry = ffs_cache_inode_alloc();
    }

    assert(entry != NULL);

    return entry;
}

static struct ffs_cache_inode *
ffs_cache_inode_find(const struct ffs_inode_entry *inode_entry)
{
    struct ffs_cache_inode *cur;

    TAILQ_FOREACH(cur, &ffs_cache_inode_list, fci_link) {
        if (cur->fci_inode.fi_inode_entry == inode_entry) {
            return cur;
        }
    }

    return NULL;
}

void
ffs_cache_inode_delete(const struct ffs_inode_entry *inode_entry)
{
    struct ffs_cache_inode *entry;

    entry = ffs_cache_inode_find(inode_entry);
    if (entry == NULL) {
        return;
    }

    TAILQ_REMOVE(&ffs_cache_inode_list, entry, fci_link);
    ffs_cache_inode_free(entry);
}

static int
ffs_cache_populate_entry(struct ffs_cache_inode *cache_inode,
                         struct ffs_inode_entry *inode_entry)
{
    int rc;

    memset(cache_inode, 0, sizeof *cache_inode);

    rc = ffs_inode_from_entry(&cache_inode->fci_inode, inode_entry);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_inode_calc_data_length(cache_inode->fci_inode.fi_inode_entry,
                                    &cache_inode->fci_file_size);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
ffs_cache_inode_ensure(struct ffs_cache_inode **out_cache_inode,
                       struct ffs_inode_entry *inode_entry)
{
    struct ffs_cache_inode *cache_inode;
    int rc;

    cache_inode = ffs_cache_inode_find(inode_entry);
    if (cache_inode != NULL) {
        rc = 0;
        goto done;
    }

    cache_inode = ffs_cache_inode_acquire();
    rc = ffs_cache_populate_entry(cache_inode, inode_entry);
    if (rc != 0) {
        goto done;
    }

    TAILQ_INSERT_HEAD(&ffs_cache_inode_list, cache_inode, fci_link);

    rc = 0;

done:
    if (rc == 0) {
        *out_cache_inode = cache_inode;
    } else {
        ffs_cache_inode_free(cache_inode);
        *out_cache_inode = NULL;
    }
    return rc;
}

static void
ffs_cache_inode_range(const struct ffs_cache_inode *cache_inode,
                      uint32_t *out_start, uint32_t *out_end)
{
    struct ffs_cache_block *cache_block;

    cache_block = TAILQ_FIRST(&cache_inode->fci_block_list);
    if (cache_block == NULL) {
        *out_start = 0;
        *out_end = 0;
        return;
    }

    *out_start = cache_block->fcb_file_offset;

    cache_block = TAILQ_LAST(&cache_inode->fci_block_list,
                             ffs_cache_block_list);
    *out_end = cache_block->fcb_file_offset +
               cache_block->fcb_block.fb_data_len;
}

static struct ffs_cache_block *
ffs_cache_block_alloc(void)
{
    struct ffs_cache_block *entry;

    entry = os_memblock_get(&ffs_cache_block_pool);
    if (entry != NULL) {
        memset(entry, 0, sizeof *entry);
    }

    return entry;
}

static void
ffs_cache_block_free(struct ffs_cache_block *entry)
{
    if (entry != NULL) {
        os_memblock_put(&ffs_cache_block_pool, entry);
    }
}

static void
ffs_cache_collect_blocks(void)
{
    struct ffs_cache_inode *cache_inode;

    TAILQ_FOREACH_REVERSE(cache_inode, &ffs_cache_inode_list,
                          ffs_cache_inode_list, fci_link) {
        if (!TAILQ_EMPTY(&cache_inode->fci_block_list)) {
            ffs_cache_inode_free_blocks(cache_inode);
            return;
        }
    }

    assert(0);
}

static struct ffs_cache_block *
ffs_cache_block_acquire(void)
{
    struct ffs_cache_block *cache_block;

    cache_block = ffs_cache_block_alloc();
    if (cache_block == NULL) {
        ffs_cache_collect_blocks();
        cache_block = ffs_cache_block_alloc();
    }

    assert(cache_block != NULL);

    return cache_block;
}

static int
ffs_cache_block_populate(struct ffs_cache_block *cache_block,
                         struct ffs_hash_entry *block_entry,
                         uint32_t end_offset)
{
    int rc;

    rc = ffs_block_from_hash_entry(&cache_block->fcb_block, block_entry);
    if (rc != 0) {
        return rc;
    }

    cache_block->fcb_file_offset = end_offset -
                                   cache_block->fcb_block.fb_data_len;

    return 0;
}

int
ffs_cache_seek(struct ffs_cache_inode *cache_inode, uint32_t to,
               struct ffs_cache_block **out_cache_block)
{
    struct ffs_cache_block *cache_block;
    struct ffs_hash_entry *block_entry;
    uint32_t cache_start;
    uint32_t cache_end;
    uint32_t offset;
    int rc;

    ffs_cache_inode_range(cache_inode, &cache_start, &cache_end);
    if (to <= cache_start) {
        cache_block = TAILQ_FIRST(&cache_inode->fci_block_list);
    } else if (to < cache_end) {
        cache_block = TAILQ_LAST(&cache_inode->fci_block_list,
                                 ffs_cache_block_list);
    } else {
        cache_block = NULL;
    }

    if (cache_block != NULL) {
        block_entry = cache_block->fcb_block.fb_hash_entry;
        offset = cache_block->fcb_file_offset;
    } else {
        block_entry =
            cache_inode->fci_inode.fi_inode_entry->fie_last_block_entry;
        offset = cache_inode->fci_file_size;
    }

    while (1) {
        if (cache_block == NULL) {
            cache_block = ffs_cache_block_acquire();
            rc = ffs_cache_block_populate(cache_block, block_entry, offset);
            if (rc != 0) {
                return rc;
            }

            TAILQ_INSERT_HEAD(&cache_inode->fci_block_list, cache_block,
                              fcb_link);

        }

        offset = cache_block->fcb_file_offset;
        if (offset <= to) {
            if (out_cache_block != NULL) {
                *out_cache_block = cache_block;
            }
            break;
        }

        block_entry = cache_block->fcb_block.fb_prev;
        cache_block = TAILQ_PREV(cache_block, ffs_cache_block_list, fcb_link);
    }

    return 0;
}

void
ffs_cache_clear(void)
{
    struct ffs_cache_inode *entry;

    while ((entry = TAILQ_FIRST(&ffs_cache_inode_list)) != NULL) {
        TAILQ_REMOVE(&ffs_cache_inode_list, entry, fci_link);
        ffs_cache_inode_free(entry);
    }
}
