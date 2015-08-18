#include <assert.h>
#include <string.h>
#include "ffs/ffs.h"
#include "ffs_priv.h"

TAILQ_HEAD(ffs_cache_list, ffs_cache_inode);
static struct ffs_cache_list ffs_cache_list =
    TAILQ_HEAD_INITIALIZER(ffs_cache_list);

static struct ffs_cache_inode *
ffs_cache_inode_alloc(void)
{
    struct ffs_cache_inode *entry;

    entry = os_memblock_get(&ffs_cache_inode_pool);
    if (entry != NULL) {
        memset(entry, 0, sizeof *entry);
    }

    return entry;
}

static void
ffs_cache_inode_free(struct ffs_cache_inode *entry)
{
    if (entry != NULL) {
        os_memblock_put(&ffs_cache_inode_pool, entry);
    }
}

static struct ffs_cache_inode *
ffs_cache_inode_acquire(void)
{
    struct ffs_cache_inode *entry;

    entry = ffs_cache_inode_alloc();
    if (entry == NULL) {
        entry = TAILQ_LAST(&ffs_cache_list, ffs_cache_list);
        assert(entry != NULL);

        TAILQ_REMOVE(&ffs_cache_list, entry, fci_link);
    }

    return entry;
}

static struct ffs_cache_inode *
ffs_cache_inode_find(const struct ffs_inode_entry *inode_entry)
{
    struct ffs_cache_inode *cur;

    TAILQ_FOREACH(cur, &ffs_cache_list, fci_link) {
        if (cur->fci_inode_entry == inode_entry) {
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

    TAILQ_REMOVE(&ffs_cache_list, entry, fci_link);
    ffs_cache_inode_free(entry);
}

static int
ffs_cache_populate_entry(struct ffs_cache_inode *entry)
{
    int rc;

    assert(entry->fci_inode_entry != NULL);

    rc = ffs_inode_calc_data_length(entry->fci_inode_entry,
                                    &entry->fci_file_size);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
ffs_cache_inode_assure(struct ffs_cache_inode **out_entry,
                       struct ffs_inode_entry *inode_entry)
{
    struct ffs_cache_inode *entry;
    int rc;

    entry = ffs_cache_inode_find(inode_entry);
    if (entry != NULL) {
        rc = 0;
        goto done;
    }

    entry = ffs_cache_inode_acquire();
    if (entry == NULL) {
        rc = FFS_ENOMEM;
        goto done;
    }

    entry->fci_inode_entry = inode_entry;
    rc = ffs_cache_populate_entry(entry);
    if (rc != 0) {
        goto done;
    }

    TAILQ_INSERT_HEAD(&ffs_cache_list, entry, fci_link);

    rc = 0;

done:
    if (rc == 0) {
        *out_entry = entry;
    } else {
        ffs_cache_inode_free(entry);
        *out_entry = NULL;
    }
    return rc;
}

void
ffs_cache_clear(void)
{
    struct ffs_cache_inode *entry;

    while ((entry = TAILQ_FIRST(&ffs_cache_list)) != NULL) {
        TAILQ_REMOVE(&ffs_cache_list, entry, fci_link);
        ffs_cache_inode_free(entry);
    }
}
