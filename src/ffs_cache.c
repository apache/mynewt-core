#include <assert.h>
#include <string.h>
#include "ffs/ffs.h"
#include "ffs_priv.h"

static void ffs_cache_block_free(struct ffs_cache_block *entry);

TAILQ_HEAD(ffs_cache_inode_list, ffs_cache_inode);
static struct ffs_cache_inode_list ffs_cache_inode_list =
    TAILQ_HEAD_INITIALIZER(ffs_cache_inode_list);


static struct ffs_hash_entry *
ffs_cache_inode_last_entry(struct ffs_cache_inode *cache_inode)
{
    struct ffs_cache_block *cache_block;

    if (TAILQ_EMPTY(&cache_inode->fci_block_list)) {
        return NULL;
    }

    cache_block = TAILQ_LAST(&cache_inode->fci_block_list,
                             ffs_cache_block_list);
    return cache_block->fcb_block.fb_hash_entry;
}

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
    struct ffs_block block;
    struct ffs_hash_entry *last_cached_entry;
    struct ffs_hash_entry *block_entry;
    struct ffs_hash_entry *pred_entry;
    uint32_t cache_start;
    uint32_t cache_end;
    uint32_t block_start;
    uint32_t block_end;
    int rc;

    ffs_cache_inode_range(cache_inode, &cache_start, &cache_end);
    if (to <= cache_start) {
        /* Seeking prior to cache.  Iterate backwards from cache start. */
        cache_block = TAILQ_FIRST(&cache_inode->fci_block_list);
    } else if (to < cache_end) {
        /* Seeking within cache.  Iterate backwards from cache end. */
        cache_block = TAILQ_LAST(&cache_inode->fci_block_list,
                                 ffs_cache_block_list);
    } else {
        /* Seeking beyond end of cache.  Iterate backwards from file end.  If
         * sought-after block is adjacent to cache end, its cache entry will
         * get appended to the current cache.  Otherwise, the current cache
         * will be freed and replaced with the single requested block.
         */
        cache_block = NULL;
    }

    if (cache_block != NULL) {
        /* We are starting the search from within the cache. */
        block_entry = cache_block->fcb_block.fb_hash_entry;
        block_end = cache_block->fcb_file_offset;
    } else {
        /* Cache is empty or user is seeking beyond end of cache.  Start
         * iteration from the end of the file.
         */
        block_entry =
            cache_inode->fci_inode.fi_inode_entry->fie_last_block_entry;
        block_end = cache_inode->fci_file_size;
    }

    while (1) {
        if (block_end < cache_start) {
            /* We are looking before the start of the cache.  Allocate a new
             * cache block and prepend it to the cache.
             */
            assert(cache_block == NULL);
            cache_block = ffs_cache_block_acquire();
            rc = ffs_cache_block_populate(cache_block, block_entry,
                                          block_end);
            if (rc != 0) {
                return rc;
            }

            TAILQ_INSERT_HEAD(&cache_inode->fci_block_list, cache_block,
                              fcb_link);
        }

        /* Calculate the file block_end of the start of this block.  This is
         * used to determine if this block contains the sought-after offset.
         */
        if (cache_block != NULL) {
            /* Current block is cached. */
            block_start = cache_block->fcb_file_offset;
            pred_entry = cache_block->fcb_block.fb_prev;
        } else {
            /* We are looking beyond the end of the cache.  Read the data block
             * from flash.
             */
            rc = ffs_block_from_hash_entry(&block, block_entry);
            if (rc != 0) {
                return rc;
            }

            block_start = block_end - block.fb_data_len;
            pred_entry = block.fb_prev;
        }

        if (block_start <= to) {
            /* This block contains the requested address; iteration is
             * complete.
             */
           if (cache_block == NULL) {
                /* The block isn't cached, so it must come after the cache end.
                 * Append it to the cache if it directly follows.  Otherwise,
                 * erase the current cache and populate it with this single
                 * block.
                 */
                cache_block = ffs_cache_block_acquire();
                cache_block->fcb_block = block;
                cache_block->fcb_file_offset = block_start;

                last_cached_entry = ffs_cache_inode_last_entry(cache_inode);
                if (last_cached_entry != NULL &&
                    last_cached_entry == pred_entry) {

                    TAILQ_INSERT_TAIL(&cache_inode->fci_block_list,
                                      cache_block, fcb_link);
                } else {
                    ffs_cache_inode_free_blocks(cache_inode);
                    TAILQ_INSERT_HEAD(&cache_inode->fci_block_list,
                                      cache_block, fcb_link);
                }
            }

            if (out_cache_block != NULL) {
                *out_cache_block = cache_block;
            }
            break;
        }

        /* Prepare for next iteration. */
        if (cache_block != NULL) {
            cache_block = TAILQ_PREV(cache_block, ffs_cache_block_list,
                                     fcb_link);
        }
        block_entry = pred_entry;
        block_end = block_start;
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
