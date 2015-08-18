#include <stddef.h>
#include <string.h>
#include <assert.h>
#include "os/os_mempool.h"
#include "ffs/ffs.h"
#include "ffs_priv.h"
#include "crc16.h"

/* Partition the flash buffer into two equal halves; used for filename
 * comparisons.
 */
#define FFS_INODE_FILENAME_BUF_SZ   (sizeof ffs_flash_buf / 2)
static uint8_t *ffs_inode_filename_buf0 = ffs_flash_buf;
static uint8_t *ffs_inode_filename_buf1 =
    ffs_flash_buf + FFS_INODE_FILENAME_BUF_SZ;

/** A list of directory inodes with pending unlink operations. */
static struct ffs_hash_list ffs_inode_unlink_list;

struct ffs_inode_entry *
ffs_inode_entry_alloc(void)
{
    struct ffs_inode_entry *inode_entry;

    inode_entry = os_memblock_get(&ffs_inode_entry_pool);
    if (inode_entry != NULL) {
        memset(inode_entry, 0, sizeof *inode_entry);
    }

    return inode_entry;
}

void
ffs_inode_entry_free(struct ffs_inode_entry *inode_entry)
{
    os_memblock_put(&ffs_inode_entry_pool, inode_entry);
}

uint32_t
ffs_inode_disk_size(const struct ffs_inode *inode)
{
    return sizeof (struct ffs_disk_inode) + inode->fi_filename_len;
}

int
ffs_inode_read_disk(uint8_t area_idx, uint32_t offset,
                    struct ffs_disk_inode *out_disk_inode)
{
    int rc;

    rc = ffs_flash_read(area_idx, offset, out_disk_inode,
                        sizeof *out_disk_inode);
    if (rc != 0) {
        return rc;
    }
    if (out_disk_inode->fdi_magic != FFS_INODE_MAGIC) {
        return FFS_EUNEXP;
    }

    return 0;
}

int
ffs_inode_write_disk(const struct ffs_disk_inode *disk_inode,
                     const char *filename, uint8_t area_idx,
                     uint32_t area_offset)
{
    int rc;

    rc = ffs_flash_write(area_idx, area_offset, disk_inode, sizeof *disk_inode);
    if (rc != 0) {
        return rc;
    }

    if (disk_inode->fdi_filename_len != 0) {
        rc = ffs_flash_write(area_idx, area_offset + sizeof *disk_inode,
                             filename, disk_inode->fdi_filename_len);
        if (rc != 0) {
            return rc;
        }
    }

#if FFS_DEBUG
    rc = ffs_crc_disk_inode_validate(disk_inode, area_idx, area_offset);
    assert(rc == 0);
#endif

    return 0;
}

int
ffs_inode_calc_data_length(struct ffs_inode_entry *inode_entry,
                           uint32_t *out_len)
{
    struct ffs_hash_entry *cur;
    struct ffs_block block;
    int rc;

    assert(ffs_hash_id_is_file(inode_entry->fie_hash_entry.fhe_id));

    *out_len = 0;

    cur = inode_entry->fie_last_block_entry;
    while (cur != NULL) {
        rc = ffs_block_from_hash_entry(&block, cur);
        if (rc != 0) {
            return rc;
        }

        *out_len += block.fb_data_len;

        cur = block.fb_prev;
    }

    return 0;
}

int
ffs_inode_from_entry(struct ffs_inode *out_inode,
                     struct ffs_inode_entry *entry)
{
    struct ffs_disk_inode disk_inode;
    uint32_t area_offset;
    uint8_t area_idx;
    int cached_name_len;
    int rc;

    ffs_flash_loc_expand(entry->fie_hash_entry.fhe_flash_loc,
                         &area_idx, &area_offset);

    rc = ffs_inode_read_disk(area_idx, area_offset, &disk_inode);
    if (rc != 0) {
        return rc;
    }

    out_inode->fi_inode_entry = entry;
    out_inode->fi_seq = disk_inode.fdi_seq;
    if (disk_inode.fdi_parent_id == FFS_ID_NONE) {
        out_inode->fi_parent = NULL;
    } else {
        out_inode->fi_parent = ffs_hash_find_inode(disk_inode.fdi_parent_id);
    }
    out_inode->fi_filename_len = disk_inode.fdi_filename_len;

    if (out_inode->fi_filename_len > FFS_SHORT_FILENAME_LEN) {
        cached_name_len = FFS_SHORT_FILENAME_LEN;
    } else {
        cached_name_len = out_inode->fi_filename_len;
    }
    if (cached_name_len != 0) {
        rc = ffs_flash_read(area_idx, area_offset + sizeof disk_inode,
                            out_inode->fi_filename, cached_name_len);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

uint32_t
ffs_inode_parent_id(const struct ffs_inode *inode)
{
    if (inode->fi_parent == NULL) {
        return FFS_ID_NONE;
    } else {
        return inode->fi_parent->fie_hash_entry.fhe_id;
    }
}

static int
ffs_inode_delete_blocks_from_ram(struct ffs_inode_entry *inode_entry)
{
    int rc;

    assert(ffs_hash_id_is_file(inode_entry->fie_hash_entry.fhe_id));

    while (inode_entry->fie_last_block_entry != NULL) {
        rc = ffs_block_delete_from_ram(inode_entry->fie_last_block_entry);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

static int
ffs_inode_delete_from_ram(struct ffs_inode_entry *inode_entry)
{
    int rc;

    if (ffs_hash_id_is_file(inode_entry->fie_hash_entry.fhe_id)) {
        rc = ffs_inode_delete_blocks_from_ram(inode_entry);
        if (rc != 0) {
            return rc;
        }
    }

    ffs_hash_remove(&inode_entry->fie_hash_entry);
    ffs_inode_entry_free(inode_entry);

    return 0;
}

/**
 * Decrements the reference count of the specified inode entry.
 *
 * @param out_next              On success, this points to the next hash entry
 *                                  that should be processed if the entire
 *                                  hash table is being iterated.  This is
 *                                  necessary because this function may result
 *                                  in multiple deletions.  Pass null if you
 *                                  don't need this information.
 */
int
ffs_inode_dec_refcnt(struct ffs_inode_entry *inode_entry)
{
    int rc;

    assert(inode_entry->fie_refcnt > 0);

    inode_entry->fie_refcnt--;
    if (inode_entry->fie_refcnt == 0) {
        rc = ffs_inode_delete_from_ram(inode_entry);
        if (rc != 0) {
            return rc;
        }
    }

    return 0;
}

/**
 * Inserts the specified inode entry into the unlink list.  Because a hash
 * entry only has a single 'next' pointer, this function removes the entry from
 * the hash table prior to inserting it into the unlink list.
 *
 * @param inode_entry           The inode entry to insert.
 */
static void
ffs_inode_insert_unlink_list(struct ffs_inode_entry *inode_entry)
{
    ffs_hash_remove(&inode_entry->fie_hash_entry);
    SLIST_INSERT_HEAD(&ffs_inode_unlink_list, &inode_entry->fie_hash_entry,
                      fhe_next);
}

/**
 * Unlinks every directory inode entry present in the unlink list.  After this
 * function completes:
 *     o Each descendant directory is deleted from RAM.
 *     o Each descendant file has its reference count decremented (and deleted
 *       from RAM if its reference count reaches zero).
 *
 * @param inout_next            This parameter is only necessary if you are
 *                                  calling this function during an iteration
 *                                  of the entire hash table; pass null
 *                                  otherwise.
 *                              On input, this points to the next hash entry
 *                                  you plan on processing.
 *                              On output, this points to the next hash entry
 *                                  that should be processed.
 */
static int
ffs_inode_process_unlink_list(struct ffs_hash_entry **inout_next)
{
    struct ffs_inode_entry *inode_entry;
    struct ffs_inode_entry *child_next;
    struct ffs_inode_entry *child;
    struct ffs_hash_entry *hash_entry;
    int rc;

    while ((hash_entry = SLIST_FIRST(&ffs_inode_unlink_list)) != NULL) {
        assert(ffs_hash_id_is_dir(hash_entry->fhe_id));

        SLIST_REMOVE_HEAD(&ffs_inode_unlink_list, fhe_next);

        inode_entry = (struct ffs_inode_entry *)hash_entry;

        /* Recursively unlink each child. */
        child = SLIST_FIRST(&inode_entry->fie_child_list);
        while (child != NULL) {
            child_next = SLIST_NEXT(child, fie_sibling_next);

            if (inout_next != NULL && *inout_next == &child->fie_hash_entry) {
                *inout_next = &child_next->fie_hash_entry;
            }

            if (ffs_hash_id_is_dir(child->fie_hash_entry.fhe_id)) {
                ffs_inode_insert_unlink_list(child);
            } else {
                rc = ffs_inode_dec_refcnt(child);
                if (rc != 0) {
                    return rc;
                }
            }

            child = child_next;
        }

        /* The directory is already removed from the hash table; just free its
         * memory.
         */
        ffs_inode_entry_free(inode_entry);
    }

    return 0;
}

int
ffs_inode_delete_from_disk(struct ffs_inode *inode)
{
    struct ffs_disk_inode disk_inode;
    uint32_t offset;
    uint8_t area_idx;
    int rc;

    /* Make sure it isn't already deleted. */
    assert(inode->fi_parent != NULL);

    rc = ffs_misc_reserve_space(sizeof disk_inode, &area_idx, &offset);
    if (rc != 0) {
        return rc;
    }

    inode->fi_seq++;

    disk_inode.fdi_magic = FFS_INODE_MAGIC;
    disk_inode.fdi_id = inode->fi_inode_entry->fie_hash_entry.fhe_id;
    disk_inode.fdi_seq = inode->fi_seq;
    disk_inode.fdi_parent_id = FFS_ID_NONE;
    disk_inode.fdi_filename_len = 0;
    ffs_crc_disk_inode_fill(&disk_inode, "");

    rc = ffs_inode_write_disk(&disk_inode, "", area_idx, offset);
    if (rc != 0) {
        return rc;
    }

    inode->fi_inode_entry->fie_hash_entry.fhe_flash_loc =
        ffs_flash_loc(area_idx, offset);

    return 0;
}

int
ffs_inode_rename(struct ffs_inode_entry *inode_entry,
                 struct ffs_inode_entry *new_parent, const char *filename)
{
    struct ffs_disk_inode disk_inode;
    struct ffs_inode inode;
    uint32_t offset;
    uint8_t area_idx;
    int filename_len;
    int rc;

    rc = ffs_inode_from_entry(&inode, inode_entry);
    if (rc != 0) {
        return rc;
    }
    inode.fi_parent = new_parent;

    filename_len = strlen(filename);
    rc = ffs_misc_reserve_space(sizeof disk_inode + filename_len,
                                &area_idx, &offset);
    if (rc != 0) {
        return rc;
    }

    disk_inode.fdi_magic = FFS_INODE_MAGIC;
    disk_inode.fdi_id = inode_entry->fie_hash_entry.fhe_id;
    disk_inode.fdi_seq = inode.fi_seq + 1;
    disk_inode.fdi_parent_id = ffs_inode_parent_id(&inode);
    disk_inode.fdi_filename_len = filename_len;
    ffs_crc_disk_inode_fill(&disk_inode, filename);

    rc = ffs_inode_write_disk(&disk_inode, filename, area_idx, offset);
    if (rc != 0) {
        return rc;
    }

    inode_entry->fie_hash_entry.fhe_flash_loc =
        ffs_flash_loc(area_idx, offset);

    return 0;
}

static int
ffs_inode_read_filename_chunk(const struct ffs_inode *inode,
                              uint8_t filename_offset, void *buf, int len)
{
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;

    assert(filename_offset + len <= inode->fi_filename_len);

    ffs_flash_loc_expand(inode->fi_inode_entry->fie_hash_entry.fhe_flash_loc,
                         &area_idx, &area_offset);
    area_offset += sizeof (struct ffs_disk_inode) + filename_offset;

    rc = ffs_flash_read(area_idx, area_offset, buf, len);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

int
ffs_inode_add_child(struct ffs_inode_entry *parent,
                    struct ffs_inode_entry *child)
{
    struct ffs_inode_entry *prev;
    struct ffs_inode_entry *cur;
    struct ffs_inode child_inode;
    struct ffs_inode cur_inode;
    int cmp;
    int rc;

    assert(ffs_hash_id_is_dir(parent->fie_hash_entry.fhe_id));

    rc = ffs_inode_from_entry(&child_inode, child);
    if (rc != 0) {
        return rc;
    }

    prev = NULL;
    SLIST_FOREACH(cur, &parent->fie_child_list, fie_sibling_next) {
        assert(cur != child);
        rc = ffs_inode_from_entry(&cur_inode, cur);
        if (rc != 0) {
            return rc;
        }

        rc = ffs_inode_filename_cmp_flash(&child_inode, &cur_inode, &cmp);
        if (rc != 0) {
            return rc;
        }

        if (cmp < 0) {
            break;
        }

        prev = cur;
    }

    if (prev == NULL) {
        SLIST_INSERT_HEAD(&parent->fie_child_list, child, fie_sibling_next);
    } else {
        SLIST_INSERT_AFTER(prev, child, fie_sibling_next);
    }

    return 0;
}

void
ffs_inode_remove_child(struct ffs_inode *child)
{
    struct ffs_inode_entry *parent;

    parent = child->fi_parent;
    assert(parent != NULL);
    assert(ffs_hash_id_is_dir(parent->fie_hash_entry.fhe_id));
    SLIST_REMOVE(&parent->fie_child_list, child->fi_inode_entry,
                 ffs_inode_entry, fie_sibling_next);
}

int
ffs_inode_filename_cmp_ram(const struct ffs_inode *inode,
                           const char *name, int name_len,
                           int *result)
{
    int short_len;
    int chunk_len;
    int rem_len;
    int off;
    int rc;

    if (name_len < inode->fi_filename_len) {
        short_len = name_len;
    } else {
        short_len = inode->fi_filename_len;
    }

    if (short_len <= FFS_SHORT_FILENAME_LEN) {
        chunk_len = short_len;
    } else {
        chunk_len = FFS_SHORT_FILENAME_LEN;
    }
    *result = strncmp((char *)inode->fi_filename, name, chunk_len);

    off = chunk_len;
    while (*result == 0 && off < short_len) {
        rem_len = short_len - off;
        if (rem_len > FFS_INODE_FILENAME_BUF_SZ) {
            chunk_len = FFS_INODE_FILENAME_BUF_SZ;
        } else {
            chunk_len = rem_len;
        }

        rc = ffs_inode_read_filename_chunk(inode, off,
                                           ffs_inode_filename_buf0,
                                           chunk_len);
        if (rc != 0) {
            return rc;
        }

        *result = strncmp((char *)ffs_inode_filename_buf0, name + off,
                          chunk_len);
        off += chunk_len;
    }

    if (*result == 0) {
        *result = inode->fi_filename_len - name_len;
    }

    return 0;
}

int
ffs_inode_filename_cmp_flash(const struct ffs_inode *inode1,
                             const struct ffs_inode *inode2,
                             int *result)
{
    int short_len;
    int chunk_len;
    int rem_len;
    int off;
    int rc;

    if (inode1->fi_filename_len < inode2->fi_filename_len) {
        short_len = inode1->fi_filename_len;
    } else {
        short_len = inode2->fi_filename_len;
    }

    if (short_len <= FFS_SHORT_FILENAME_LEN) {
        chunk_len = short_len;
    } else {
        chunk_len = FFS_SHORT_FILENAME_LEN;
    }
    *result = strncmp((char *)inode1->fi_filename,
                      (char *)inode2->fi_filename,
                      chunk_len);

    off = chunk_len;
    while (*result == 0 && off < short_len) {
        rem_len = short_len - off;
        if (rem_len > FFS_INODE_FILENAME_BUF_SZ) {
            chunk_len = FFS_INODE_FILENAME_BUF_SZ;
        } else {
            chunk_len = rem_len;
        }

        rc = ffs_inode_read_filename_chunk(inode1, off,
                                           ffs_inode_filename_buf0,
                                           chunk_len);
        if (rc != 0) {
            return rc;
        }

        rc = ffs_inode_read_filename_chunk(inode2, off,
                                           ffs_inode_filename_buf1,
                                           chunk_len);
        if (rc != 0) {
            return rc;
        }

        *result = strncmp((char *)ffs_inode_filename_buf0,
                          (char *)ffs_inode_filename_buf1,
                          chunk_len);
        off += chunk_len;
    }

    if (*result == 0) {
        *result = inode1->fi_filename_len - inode2->fi_filename_len;
    }

    return 0;
}

/**
 * Finds the set of blocks composing the specified address range within the
 * supplied file inode.  This information is returned in the form of a
 * seek_info object.
 *
 * @param inode_entry           The file inode to seek within.
 * @param offset                The start address of the region to seek to.
 * @param length                The length of the region to seek to.
 * @param out_seek_info         On success, this gets populated with the result
 *                                  of the seek operation.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_inode_seek(struct ffs_inode_entry *inode_entry, uint32_t offset,
               uint32_t length, struct ffs_seek_info *out_seek_info)
{
    struct ffs_hash_entry *cur_entry;
    struct ffs_block block;
    uint32_t block_start;
    uint32_t cur_offset;
    uint32_t file_len;
    uint32_t seek_end;
    int rc;

    assert(ffs_hash_id_is_file(inode_entry->fie_hash_entry.fhe_id));

    rc = ffs_inode_calc_data_length(inode_entry, &file_len);
    if (rc != 0) {
        return rc;
    }

    if (offset > file_len) {
        return FFS_ERANGE;
    }
    if (offset == file_len) {
        memset(&out_seek_info->fsi_last_block, 0,
               sizeof out_seek_info->fsi_last_block);
        out_seek_info->fsi_last_block.fb_hash_entry = NULL;
        out_seek_info->fsi_block_file_off = 0;
        out_seek_info->fsi_file_len = file_len;
        return 0;
    }

    seek_end = offset + length;

    cur_entry = inode_entry->fie_last_block_entry;
    cur_offset = file_len;
    while (1) {
        rc = ffs_block_from_hash_entry(&block, cur_entry);
        if (rc != 0) {
            return rc;
        }

        block_start = cur_offset - block.fb_data_len;
        if (seek_end > block_start) {
            out_seek_info->fsi_last_block = block;
            out_seek_info->fsi_block_file_off = block_start;
            out_seek_info->fsi_file_len = file_len;
            return 0;
        }

        cur_offset = block_start;
        cur_entry = block.fb_prev;
    }
}

/**
 * Reads data from the specified file inode.
 *
 * @param inode_entry           The inode to read from.
 * @param offset                The offset within the file to start the read
 *                                  at.
 * @param data                  On success, the read data gets written here.
 * @param len
 */
int
ffs_inode_read(struct ffs_inode_entry *inode_entry, uint32_t offset,
               uint32_t len, void *out_data, uint32_t *out_len)
{
    struct ffs_seek_info seek_info;
    struct ffs_block block;
    uint32_t src_start;
    uint32_t src_end;
    uint32_t dst_off;
    uint32_t area_offset;
    uint32_t block_start;
    uint32_t read_len;
    uint16_t block_off;
    uint16_t chunk_len;
    uint8_t *dst;
    uint8_t area_idx;
    int rc;

    dst = out_data;

    rc = ffs_inode_seek(inode_entry, offset, len, &seek_info);
    if (rc != 0) {
        return rc;
    }

    if (seek_info.fsi_last_block.fb_hash_entry == NULL) {
        *out_len = 0;
        return 0;
    }

    block = seek_info.fsi_last_block;
    block_start = seek_info.fsi_block_file_off;
    src_start = offset;

    if (block_start + seek_info.fsi_last_block.fb_data_len > len) {
        src_end = offset + len;
    } else {
        src_end = block_start + seek_info.fsi_last_block.fb_data_len;
    }

    read_len = src_end - src_start;
    dst_off = read_len;

    while (1) {
        if (block_start < src_start) {
            chunk_len = src_end - src_start;
            block_off = src_start - block_start;
        } else {
            chunk_len = src_end - block_start;
            block_off = 0;
        }
        assert(chunk_len <= dst_off);
        dst_off -= chunk_len;

        ffs_flash_loc_expand(block.fb_hash_entry->fhe_flash_loc,
                             &area_idx, &area_offset);
        area_offset += sizeof (struct ffs_disk_block);
        area_offset += block_off;

        rc = ffs_flash_read(area_idx, area_offset, dst + dst_off, chunk_len);
        if (rc != 0) {
            return rc;
        }

        if (dst_off == 0) {
            break;
        }

        src_end -= chunk_len;

        assert(block.fb_prev != NULL);
        rc = ffs_block_from_hash_entry(&block, block.fb_prev);
        if (rc != 0) {
            return rc;
        }

        block_start -= block.fb_data_len;
    }

    *out_len = read_len;

    return 0;
}

int
ffs_inode_unlink_from_ram(struct ffs_inode *inode,
                          struct ffs_hash_entry **out_next)
{
    int rc;

    if (inode->fi_parent != NULL) {
        ffs_inode_remove_child(inode);
    }

    if (ffs_hash_id_is_dir(inode->fi_inode_entry->fie_hash_entry.fhe_id)) {
        ffs_inode_insert_unlink_list(inode->fi_inode_entry);
        rc = ffs_inode_process_unlink_list(out_next);
    } else {
        rc = ffs_inode_dec_refcnt(inode->fi_inode_entry);
    }
    if (rc != 0) {
        return rc;
    }

    return 0;
}

/**
 * Unlinks the file or directory represented by the specified inode.  If the
 * inode represents a directory, all the directory's descendants are
 * recursively unlinked.  Any open file handles refering to an unlinked file
 * remain valid, and can be read from and written to.
 *
 * When an inode is unlinked, the following events occur:
 *     o inode deletion record is written to disk.
 *     o inode is removed from parent's child list.
 *     o inode's reference count is decreased (if this brings it to zero, the
 *       inode is fully deleted from RAM).
 *
 * @param inode             The inode to unlink.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
ffs_inode_unlink(struct ffs_inode *inode)
{
    int rc;

    rc = ffs_inode_delete_from_disk(inode);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_inode_unlink_from_ram(inode, NULL);
    if (rc != 0) {
        return rc;
    }

    return 0;
}
