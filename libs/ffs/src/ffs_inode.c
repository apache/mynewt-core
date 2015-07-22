#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "os/os_mempool.h"
#include "ffs/ffs.h"
#include "ffs_priv.h"

struct ffs_inode *
ffs_inode_alloc(void)
{
    struct ffs_inode *inode;

    inode = os_memblock_get(&ffs_inode_pool);
    if (inode != NULL) {
        memset(inode, 0, sizeof *inode);
    }

    inode->fi_base.fb_type = FFS_OBJECT_TYPE_INODE;

    return inode;
}

void
ffs_inode_free(struct ffs_inode *inode)
{
    os_memblock_put(&ffs_inode_pool, inode);
}

uint32_t
ffs_inode_calc_data_length(const struct ffs_inode *inode)
{
    const struct ffs_block *block;
    uint32_t len;

    assert(!(inode->fi_flags & FFS_INODE_F_DIRECTORY));

    len = 0;
    SLIST_FOREACH(block, &inode->fi_block_list, fb_next) {
        len += block->fb_data_len;
    }

    return len;
}

uint32_t
ffs_inode_parent_id(const struct ffs_inode *inode)
{
    if (inode->fi_parent == NULL) {
        return FFS_ID_NONE;
    } else {
        return inode->fi_parent->fi_base.fb_id;
    }
}

void
ffs_inode_insert_block(struct ffs_inode *inode, struct ffs_block *block)
{
    struct ffs_block *prev;
    struct ffs_block *cur;

    assert(!(inode->fi_flags & FFS_INODE_F_DIRECTORY));

    prev = NULL;
    SLIST_FOREACH(cur, &inode->fi_block_list, fb_next) {
        assert(block->fb_base.fb_id != cur->fb_base.fb_id);
        assert(block->fb_rank != cur->fb_rank);
        if (block->fb_rank < cur->fb_rank) {
            break;
        }

        prev = cur;
    }

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&inode->fi_block_list, block, fb_next);
    } else {
        SLIST_INSERT_AFTER(prev, block, fb_next);
    }

    inode->fi_data_len += block->fb_data_len;
}

static struct ffs_inode_list ffs_inode_delete_list;

static void
ffs_inode_dec_refcnt_only(struct ffs_inode *inode)
{
    assert(inode->fi_refcnt > 0);
    inode->fi_refcnt--;
    if (inode->fi_refcnt == 0) {
        if (inode->fi_parent != NULL) {
            ffs_inode_remove_child(inode);
        }
        SLIST_INSERT_HEAD(&ffs_inode_delete_list, inode, fi_sibling_next);
    }
}

void
ffs_inode_delete_from_ram(struct ffs_inode *inode)
{
    struct ffs_inode *child;
    struct ffs_block *block;

    if (inode->fi_flags & FFS_INODE_F_DIRECTORY) {
        while ((child = SLIST_FIRST(&inode->fi_child_list)) != NULL) {
            ffs_inode_dec_refcnt_only(child);
        }
    } else {
        while ((block = SLIST_FIRST(&inode->fi_block_list)) != NULL) {
            ffs_block_delete_from_ram(block);
        }
    }

    ffs_hash_remove(&inode->fi_base);

    if (inode->fi_parent != NULL) {
        ffs_inode_remove_child(inode);
    }

    ffs_inode_free(inode);
}

void
ffs_inode_dec_refcnt(struct ffs_inode *inode)
{
    struct ffs_inode *cur;

    ffs_inode_dec_refcnt_only(inode);

    while ((cur = SLIST_FIRST(&ffs_inode_delete_list)) != NULL) {
        SLIST_REMOVE(&ffs_inode_delete_list, cur, ffs_inode, fi_sibling_next);
        ffs_inode_delete_from_ram(cur);
    }
}

int
ffs_inode_delete_from_disk(const struct ffs_inode *inode)
{
    struct ffs_disk_inode disk_inode;
    uint32_t offset;
    uint16_t sector_id;
    int rc;

    rc = ffs_reserve_space(&sector_id, &offset, sizeof disk_inode);
    if (rc != 0) {
        return rc;
    }

    disk_inode.fdi_magic = FFS_INODE_MAGIC;
    disk_inode.fdi_id = inode->fi_base.fb_id;
    disk_inode.fdi_seq = inode->fi_base.fb_seq + 1;
    disk_inode.fdi_parent_id = ffs_inode_parent_id(inode);
    disk_inode.fdi_flags = inode->fi_flags | FFS_INODE_F_DELETED;
    disk_inode.fdi_filename_len = 0;

    rc = ffs_inode_write_disk(&disk_inode, "", sector_id, offset);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
ffs_inode_from_disk(struct ffs_inode *out_inode,
                    const struct ffs_disk_inode *disk_inode,
                    uint16_t sector_id, uint32_t offset)
{
    int cached_name_len;
    int rc;

    out_inode->fi_base.fb_type = FFS_OBJECT_TYPE_INODE;
    out_inode->fi_base.fb_id = disk_inode->fdi_id;
    out_inode->fi_base.fb_seq = disk_inode->fdi_seq;
    out_inode->fi_base.fb_sector_id = sector_id;
    out_inode->fi_base.fb_offset = offset;
    out_inode->fi_flags = disk_inode->fdi_flags;
    out_inode->fi_filename_len = disk_inode->fdi_filename_len;

    if (out_inode->fi_filename_len > FFS_SHORT_FILENAME_LEN) {
        cached_name_len = FFS_SHORT_FILENAME_LEN;
    } else {
        cached_name_len = out_inode->fi_filename_len;
    }
    rc = ffs_flash_read(sector_id, offset + sizeof *disk_inode,
                        out_inode->fi_filename, cached_name_len);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
ffs_inode_rename(struct ffs_inode *inode, const char *filename)
{
    struct ffs_disk_inode disk_inode;
    uint32_t offset;
    uint16_t sector_id;
    int filename_len;
    int rc;

    filename_len = strlen(filename);
    rc = ffs_reserve_space(&sector_id, &offset,
                           sizeof disk_inode + filename_len);
    if (rc != 0) {
        return rc;
    }

    disk_inode.fdi_magic = FFS_INODE_MAGIC;
    disk_inode.fdi_id = inode->fi_base.fb_id;
    disk_inode.fdi_seq = inode->fi_base.fb_seq + 1;
    disk_inode.fdi_parent_id = ffs_inode_parent_id(inode);
    disk_inode.fdi_flags = inode->fi_flags;
    disk_inode.fdi_filename_len = filename_len;

    rc = ffs_inode_write_disk(&disk_inode, filename, sector_id, offset);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_inode_from_disk(inode, &disk_inode, sector_id, offset);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
ffs_inode_read_disk(struct ffs_disk_inode *out_disk_inode, char *out_filename,
                    uint16_t sector_id, uint32_t offset)
{
    int rc;

    rc = ffs_flash_read(sector_id, offset, out_disk_inode,
                        sizeof *out_disk_inode);
    if (rc != 0) {
        return rc;
    }
    if (out_disk_inode->fdi_magic != FFS_INODE_MAGIC) {
        return FFS_EUNEXP;
    }

    /* XXX: General inode validation. */
    if (out_filename != NULL) {
        rc = ffs_flash_read(sector_id, offset + sizeof *out_disk_inode,
                            out_filename, out_disk_inode->fdi_filename_len);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

int
ffs_inode_write_disk(const struct ffs_disk_inode *disk_inode,
                     const char *filename, uint16_t sector_id,
                     uint32_t offset)
{
    int rc;

    rc = ffs_flash_write(sector_id, offset, disk_inode, sizeof *disk_inode);
    if (rc != 0) {
        return rc;
    }
    rc = ffs_flash_write(sector_id, offset + sizeof *disk_inode, filename,
                         disk_inode->fdi_filename_len);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
ffs_inode_seek(const struct ffs_inode *inode, uint32_t offset,
               struct ffs_block **out_prev_block,
               struct ffs_block **out_block, uint32_t *out_block_off)
{
    struct ffs_block *prev_block;
    struct ffs_block *block;

    prev_block = NULL;
    block = SLIST_FIRST(&inode->fi_block_list);
    while (block != NULL && offset >= block->fb_data_len) {
        offset -= block->fb_data_len;

        prev_block = block;
        block = SLIST_NEXT(block, fb_next);
    }

    if (block == NULL && offset != 0) {
        return FFS_ERANGE;
    }

    if (out_prev_block != NULL) {
        *out_prev_block = prev_block;
    }
    if (out_block != NULL) {
        *out_block = block;
    }
    if (out_block_off != NULL) {
        *out_block_off = offset;
    }

    return 0;
}

void
ffs_inode_add_child(struct ffs_inode *parent, struct ffs_inode *child)
{
    assert(parent->fi_flags & FFS_INODE_F_DIRECTORY);
    assert(child->fi_parent == NULL);
    /* XXX: Sort alphabetically. */
    SLIST_INSERT_HEAD(&parent->fi_child_list, child, fi_sibling_next);
    child->fi_parent = parent;
}

void
ffs_inode_remove_child(struct ffs_inode *child)
{
    struct ffs_inode *parent;

    parent = child->fi_parent;
    assert(parent != NULL);
    assert(parent->fi_flags & FFS_INODE_F_DIRECTORY);
    assert(child->fi_parent == parent);
    SLIST_REMOVE(&parent->fi_child_list, child, ffs_inode, fi_sibling_next);
    child->fi_parent = NULL;
}

int
ffs_inode_is_root(const struct ffs_disk_inode *disk_inode)
{
    return disk_inode->fdi_parent_id == FFS_ID_NONE         &&
           disk_inode->fdi_flags & FFS_INODE_F_DIRECTORY    &&
           !(disk_inode->fdi_flags & FFS_INODE_F_DELETED)   &&
           disk_inode->fdi_filename_len == 0;
}

int
ffs_inode_filename_cmp(int *result, const struct ffs_inode *inode,
                       const char *name, int name_len)
{
    static uint8_t buf[128];
    uint32_t sector_off;
    int chunk_len;
    int rem_len;
    int off;
    int rc;

    if (inode->fi_filename_len != name_len) {
        *result = inode->fi_filename_len - name_len;
        return 0;
    }


    if (name_len <= FFS_SHORT_FILENAME_LEN) {
        chunk_len = name_len;
    } else {
        chunk_len = FFS_SHORT_FILENAME_LEN;
    }
    *result = strncmp((char *)inode->fi_filename, name, chunk_len);

    /* If filename is short, it is fulled cached in RAM. */
    if (name_len <= FFS_SHORT_FILENAME_LEN) {
        return 0;
    }

    /* Otherwise, read the filename from flash. */
    off = FFS_SHORT_FILENAME_LEN;
    while (off < name_len) {
        rem_len = name_len - off;
        if (rem_len > sizeof buf) {
            chunk_len = sizeof buf;
        } else {
            chunk_len = rem_len;
        }

        sector_off = inode->fi_base.fb_offset +
                     sizeof (struct ffs_disk_inode) +
                     off;
        rc = ffs_flash_read(inode->fi_base.fb_sector_id,
                            sector_off, buf, chunk_len);
        if (rc != 0) {
            return rc;
        }

        *result = strncmp((char *)buf, name + off, chunk_len);
        if (*result != 0) {
            break;
        }

        off += chunk_len;
    }

    return 0;
}

