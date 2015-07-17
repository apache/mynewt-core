#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "os/os_mempool.h"
#include "os/os_mutex.h"
#include "ffs_priv.h"
#include "ffs/ffs.h"

#define FFS_NUM_FILES           8
#define FFS_NUM_INODES          2048
#define FFS_NUM_BLOCKS          10240

struct ffs_sector_info *ffs_sectors;
int ffs_num_sectors;
uint16_t ffs_scratch_sector_id;

struct os_mempool ffs_file_pool;
struct os_mempool ffs_inode_pool;
struct os_mempool ffs_block_pool;

struct ffs_inode *ffs_root_dir;
uint32_t ffs_next_id;

static struct os_mutex ffs_mutex;

static void
ffs_lock(void)
{
#if 0
    int rc;

    rc = os_mutex_pend(&ffs_mutex, 0xffffffff);
    assert(rc == 0);
#endif
}

static void
ffs_unlock(void)
{
#if 0
    int rc;

    rc = os_mutex_release(&ffs_mutex);
    assert(rc == 0);
#endif
}

static int
ffs_reserve_space_sector(uint32_t *out_offset, uint16_t sector_id,
                         uint16_t size)
{
    const struct ffs_sector_info *sector;
    uint32_t space;

    sector = ffs_sectors + sector_id;
    space = sector->fsi_length - sector->fsi_cur;
    if (space >= size) {
        *out_offset = sector->fsi_cur;
        return 0;
    }

    return FFS_EFULL;
}

int
ffs_reserve_space(uint16_t *out_sector_id, uint32_t *out_offset,
                  uint16_t size)
{
    uint16_t sector_id;
    int rc;
    int i;

    for (i = 0; i < ffs_num_sectors; i++) {
        if (i != ffs_scratch_sector_id) {
            rc = ffs_reserve_space_sector(out_offset, i, size);
            if (rc == 0) {
                *out_sector_id = i;
                return 0;
            }
        }
    }

    rc = ffs_gc(&sector_id);
    if (rc != 0) {
        return rc;
    }

    rc = ffs_reserve_space_sector(out_offset, sector_id, size);
    if (rc != 0) {
        return rc;
    }

    *out_sector_id = sector_id;

    return rc;
}

int
ffs_close(struct ffs_file *file)
{
    int rc;

    ffs_lock();

    ffs_inode_dec_refcnt(file->ff_inode);

    rc = os_memblock_put(&ffs_file_pool, file);
    if (rc != 0) {
        rc = FFS_EOS;
        goto done;
    }

    rc = 0;

done:
    ffs_unlock();
    return rc;
}

static int
ffs_unlink_internal(const char *filename)
{
    struct ffs_inode *inode;
    int rc;

    rc = ffs_path_find_inode(&inode, filename);
    if (rc != 0) {
        return rc;
    }

    if (inode->fi_refcnt > 1) {
        if (inode->fi_parent != NULL) {
            assert(inode->fi_parent->fi_flags & FFS_INODE_F_DIRECTORY);
            SLIST_REMOVE(&inode->fi_parent->fi_child_list, inode, ffs_inode,
                         fi_sibling_next);
            inode->fi_parent = NULL;
        }
    }

    rc = ffs_inode_delete_from_disk(inode);
    if (rc != 0) {
        return rc;
    }

    ffs_inode_dec_refcnt(inode);

    return 0;
}

int
ffs_unlink(const char *filename)
{
    int rc;

    ffs_lock();
    rc = ffs_unlink_internal(filename);
    ffs_unlock();

    return rc;
}

int
ffs_seek(struct ffs_file *file, uint32_t offset)
{
    int rc;

    ffs_lock();

    if (offset > file->ff_inode->fi_data_len) {
        rc = FFS_ERANGE;
        goto done;
    }

    file->ff_offset = offset;

    rc = 0;

done:
    ffs_unlock();
    return rc;
}

uint32_t
ffs_getpos(const struct ffs_file *file)
{
    uint32_t offset;

    ffs_lock();
    offset = file->ff_offset;
    ffs_unlock();

    return offset;
}

uint32_t
ffs_file_len(const struct ffs_file *file)
{
    uint32_t len;

    ffs_lock();
    len = file->ff_inode->fi_data_len;
    ffs_unlock();

    return len;
}

int
ffs_new_file(struct ffs_inode **out_inode, struct ffs_inode *parent,
             const char *filename, int is_dir)
{
    struct ffs_disk_inode disk_inode;
    struct ffs_inode *inode;
    uint16_t sector_id;
    uint32_t offset;
    int filename_len;
    int rc;

    inode = ffs_inode_alloc();
    if (inode == NULL) {
        rc = FFS_ENOMEM;
        goto err;
    }

    filename_len = strlen(filename);
    rc = ffs_reserve_space(&sector_id, &offset,
                           sizeof disk_inode + filename_len);
    if (rc != 0) {
        goto err;
    }

    memset(&disk_inode, 0xff, sizeof disk_inode);
    disk_inode.fdi_magic = FFS_INODE_MAGIC;
    disk_inode.fdi_id = ffs_next_id++;
    disk_inode.fdi_seq = 0;
    if (parent == NULL) {
        disk_inode.fdi_parent_id = FFS_ID_NONE;
    } else {
        disk_inode.fdi_parent_id = parent->fi_base.fb_id;
    }
    disk_inode.fdi_flags = 0;
    if (is_dir) {
        disk_inode.fdi_flags |= FFS_INODE_F_DIRECTORY;
    }
    disk_inode.fdi_filename_len = filename_len;

    rc = ffs_inode_write_disk(&disk_inode, filename, sector_id, offset);
    if (rc != 0) {
        goto err;
    }

    rc = ffs_inode_from_disk(inode, &disk_inode, sector_id, offset);
    if (rc != 0) {
        goto err;
    }
    if (parent != NULL) {
        SLIST_INSERT_HEAD(&parent->fi_child_list, inode, fi_sibling_next);
        inode->fi_parent = parent;
    }
    inode->fi_refcnt = 1;
    inode->fi_data_len = 0;

    ffs_hash_insert(&inode->fi_base);
    *out_inode = inode;

    return 0;

err:
    ffs_inode_free(inode);
    return rc;
}

int
ffs_open(struct ffs_file **out_file, const char *filename,
         uint8_t access_flags)
{
    struct ffs_path_parser parser;
    struct ffs_inode *parent;
    struct ffs_inode *inode;
    struct ffs_file *file;
    int rc;
    int i;

    ffs_lock();

    file = NULL;

    /* XXX: Validate arguments. */
    if (!(access_flags & (FFS_ACCESS_READ | FFS_ACCESS_WRITE))) {
        rc = FFS_EINVAL;
        goto done;
    }
    if (access_flags & FFS_ACCESS_APPEND &&
        !(access_flags & FFS_ACCESS_WRITE)) {

        rc = FFS_EINVAL;
        goto done;
    }
    if (access_flags & FFS_ACCESS_APPEND &&
        access_flags & FFS_ACCESS_TRUNCATE) {

        rc = FFS_EINVAL;
        goto done;
    }

    file = os_memblock_get(&ffs_file_pool);
    if (file == NULL) {
        rc = FFS_ENOMEM;
        goto done;
    }

    ffs_path_parser_new(&parser, filename);
    rc = ffs_path_find(&parser, &inode, &parent);
    if (rc == FFS_ENOENT) {
        if (parent == NULL || !(access_flags & FFS_ACCESS_WRITE)) {
            goto done;
        }

        assert(parser.fpp_token_type == FFS_PATH_TOKEN_LEAF); // XXX
        rc = ffs_new_file(&file->ff_inode, parent, parser.fpp_token, 0);
        if (rc != 0) {
            goto done;
        }
    } else if (access_flags & FFS_ACCESS_TRUNCATE) {
        ffs_unlink_internal(filename);
        rc = ffs_new_file(&file->ff_inode, parent, parser.fpp_token, 0);
        if (rc != 0) {
            goto done;
        }
    } else {
        file->ff_inode = inode;
    }

    if (access_flags & FFS_ACCESS_APPEND) {
        file->ff_offset = file->ff_inode->fi_data_len;
    } else {
        file->ff_offset = 0;
    }
    file->ff_inode->fi_refcnt++;
    file->ff_access_flags = access_flags;

    *out_file = file;
    rc = 0;

done:
    if (rc != 0) {
        os_memblock_put(&ffs_file_pool, file);
    }
    ffs_unlock();
    return rc;
}

int
ffs_rename(const char *from, const char *to)
{
    struct ffs_path_parser parser;
    struct ffs_inode *from_parent;
    struct ffs_inode *from_inode;
    struct ffs_inode *to_parent;
    struct ffs_inode *to_inode;
    int rc;

    ffs_lock();

    ffs_path_parser_new(&parser, from);
    rc = ffs_path_find(&parser, &from_inode, &from_parent);
    if (rc != 0) {
        goto done;
    }

    ffs_path_parser_new(&parser, to);
    rc = ffs_path_find(&parser, &to_inode, &to_parent);
    switch (rc) {
    case 0:
        /* The user is clobbering something with the rename. */
        if ((from_inode->fi_flags & FFS_INODE_F_DIRECTORY) ^
            (to_inode->fi_flags   & FFS_INODE_F_DIRECTORY)) {

            /* Cannot clobber one type of file with another. */
            rc = EINVAL;
            goto done;
        }

        ffs_inode_dec_refcnt(to_inode);
        break;

    case FFS_ENOENT:
        assert(to_parent != NULL);
        if (parser.fpp_token_type != FFS_PATH_TOKEN_LEAF) {
            /* Intermediate directory doesn't exist. */
            rc = EINVAL;
            goto done;
        }
        break;

    default:
        goto done;
    }

    if (from_parent != to_parent) {
        if (from_parent != NULL) {
            ffs_inode_remove_child(from_inode);
        }
        if (to_parent != NULL) {
            ffs_inode_add_child(to_parent, from_inode);
        }
    }

    rc = ffs_inode_rename(from_inode, parser.fpp_token);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_unlock();
    return rc;
}

struct ffs_write_info {
    struct ffs_block *fwi_prev_block;
    struct ffs_block *fwi_start_block;
    struct ffs_block *fwi_end_block;
    uint32_t fwi_start_offset;
    uint32_t fwi_end_offset;
    uint32_t fwi_extra_length;
};

static int
ffs_calc_write_overlay(struct ffs_write_info *out_write_info,
                       const struct ffs_inode *inode, 
                       uint32_t file_offset, uint32_t write_len)
{
    struct ffs_block *block;
    uint32_t block_offset;
    uint32_t chunk_len;
    int rc;

    rc = ffs_inode_seek(inode, file_offset,
                        &out_write_info->fwi_prev_block,
                        &out_write_info->fwi_start_block,
                        &out_write_info->fwi_start_offset);
    if (rc != 0) {
        return rc;
    }

    block_offset = out_write_info->fwi_start_offset;
    block = out_write_info->fwi_start_block;
    while (1) {
        if (block == NULL) {
            out_write_info->fwi_end_block = NULL;
            out_write_info->fwi_end_offset = 0;
            out_write_info->fwi_extra_length = write_len;
            return 0;
        }

        chunk_len = block->fb_data_len - block_offset;
        if (chunk_len > write_len) {
            out_write_info->fwi_end_block = block;
            out_write_info->fwi_end_offset = block_offset + write_len;
            out_write_info->fwi_extra_length = 0;
            return 0;
        }

        write_len -= chunk_len;
        block_offset = 0;
        block = SLIST_NEXT(block, fb_next);
    }
}

static uint32_t
ffs_write_disk_size(const struct ffs_write_info *write_info,
                    const struct ffs_disk_block *data_disk_block)
{
    const struct ffs_block *block;
    uint32_t size;

    /* New data disk block. */
    size = sizeof *data_disk_block + data_disk_block->fdb_data_len;

    /* Accumulate sizes of delete blocks. */
    if (write_info->fwi_start_block != NULL) {
        block = SLIST_NEXT(write_info->fwi_start_block, fb_next);
        while (1) {
            if (block == NULL) {
                break;
            }

            size += sizeof (struct ffs_disk_block);

            if (block == write_info->fwi_end_block) {
                break;
            }

            block = SLIST_NEXT(block, fb_next);
        }
    }

    return size;
}

static int
ffs_write_gen(const struct ffs_write_info *write_info, struct ffs_inode *inode,
              const void *data, int data_len)
{
    struct ffs_disk_block disk_block;
    struct ffs_block *block;
    struct ffs_block *next;
    uint32_t expected_cur;
    uint32_t disk_size;
    uint32_t chunk_len;
    uint32_t offset;
    uint32_t cur;
    uint16_t sector_id;
    int rc;

    disk_block.fdb_data_len = data_len;
    if (write_info->fwi_start_block != NULL) {
        disk_block.fdb_data_len += write_info->fwi_start_offset;
        disk_block.fdb_id = write_info->fwi_start_block->fb_base.fb_id;
        disk_block.fdb_seq = write_info->fwi_start_block->fb_base.fb_seq + 1;
        disk_block.fdb_rank = write_info->fwi_start_block->fb_rank;
    } else {
        disk_block.fdb_id = ffs_next_id++;
        disk_block.fdb_seq = 0;
        if (write_info->fwi_prev_block != NULL) {
            disk_block.fdb_rank = write_info->fwi_prev_block->fb_rank + 1;
        } else {
            disk_block.fdb_rank = 0;
        }
    }

    if (write_info->fwi_end_block != NULL) {
        disk_block.fdb_data_len += write_info->fwi_end_block->fb_data_len -
                                   write_info->fwi_end_offset;
    }

    disk_block.fdb_magic = FFS_BLOCK_MAGIC;
    disk_block.fdb_inode_id = inode->fi_base.fb_id;
    disk_block.reserved16 = 0;
    disk_block.fdb_flags = 0;
    disk_block.fdb_ecc = 0;

    disk_size = ffs_write_disk_size(write_info, &disk_block);
    rc = ffs_reserve_space(&sector_id, &offset, disk_size);
    if (rc != 0) {
        return rc;
    }
    expected_cur = ffs_sectors[sector_id].fsi_cur + disk_size;

    rc = ffs_flash_write(sector_id, offset, &disk_block, sizeof disk_block);
    if (rc != 0) {
        return rc;
    }

    cur = offset + sizeof disk_block;

    if (write_info->fwi_start_block != NULL) {
        chunk_len = write_info->fwi_start_offset;
        rc = ffs_flash_copy(
            write_info->fwi_start_block->fb_base.fb_sector_id,
            write_info->fwi_start_block->fb_base.fb_offset + sizeof disk_block,
            sector_id, cur, chunk_len);
        if (rc != 0) {
            return rc;
        }
        cur += chunk_len;
    }

    rc = ffs_flash_write(sector_id, cur, data, data_len);
    if (rc != 0) {
        return rc;
    }
    cur += data_len;

    if (write_info->fwi_end_block != NULL) {
        chunk_len = write_info->fwi_end_block->fb_data_len -
                    write_info->fwi_end_offset;
        rc = ffs_flash_copy(
            write_info->fwi_end_block->fb_base.fb_sector_id,
            write_info->fwi_end_block->fb_base.fb_offset +
                sizeof disk_block + write_info->fwi_end_offset,
            sector_id, cur, chunk_len);
        if (rc != 0) {
            return rc;
        }
        cur += chunk_len;
    }

    assert(cur - offset == sizeof disk_block + disk_block.fdb_data_len);

    if (write_info->fwi_start_block != NULL) {
        next = SLIST_NEXT(write_info->fwi_start_block, fb_next);
        if (next != NULL) {
            ffs_block_delete_list_from_disk(next,
                                            write_info->fwi_end_block);
        }
        ffs_block_delete_list_from_ram(write_info->fwi_start_block,
                                       write_info->fwi_end_block);
    }

    block = ffs_block_alloc();
    if (block == NULL) {
        return FFS_ENOMEM;
    }

    ffs_block_from_disk(block, &disk_block, sector_id, offset);
    block->fb_inode = inode;
    ffs_hash_insert(&block->fb_base);
    ffs_inode_insert_block(inode, block);

    assert(ffs_sectors[sector_id].fsi_cur == expected_cur);

    return 0;
}

int
ffs_write(struct ffs_file *file, const void *data, int len)
{
    struct ffs_write_info write_info;
    uint32_t file_offset;
    int rc;

    ffs_lock();

    if (!(file->ff_access_flags & FFS_ACCESS_WRITE)) {
        rc = FFS_ERDONLY;
        goto done;
    }

    if (file->ff_access_flags & FFS_ACCESS_APPEND) {
        file_offset = file->ff_inode->fi_data_len;
    } else {
        file_offset = file->ff_offset;
    }

    rc = ffs_calc_write_overlay(&write_info, file->ff_inode, file_offset, len);
    if (rc != 0) {
        goto done;
    }

    rc = ffs_write_gen(&write_info, file->ff_inode, data, len);
    if (rc != 0) {
        goto done;
    }

    file->ff_offset = file_offset + len;

    rc = 0;

done:
    ffs_unlock();
    return rc;
}

int
ffs_read(struct ffs_file *file, void *data, uint32_t *len)
{
    struct ffs_block *block;
    uint32_t bytes_read;
    uint32_t bytes_left;
    uint32_t sector_off;
    uint32_t block_off;
    uint32_t chunk_len;
    uint8_t *dst;
    int rc;

    ffs_lock();

    dst = data;
    bytes_read = 0;
    bytes_left = *len;

    rc = ffs_inode_seek(file->ff_inode, file->ff_offset, NULL, &block,
                        &block_off);
    if (rc != 0) {
        goto done;
    }


    while (block != NULL && bytes_left > 0) {
        if (bytes_left > block->fb_data_len) {
            chunk_len = block->fb_data_len;
        } else {
            chunk_len = bytes_left;
        }

        sector_off = block->fb_base.fb_offset +
                     sizeof (struct ffs_disk_block) +
                     block_off;
        rc = ffs_flash_read(block->fb_base.fb_sector_id, sector_off, dst,
                            chunk_len);
        if (rc != 0) {
            goto done;
        }

        dst += chunk_len;
        bytes_read += chunk_len;
        bytes_left -= chunk_len;
        block = SLIST_NEXT(block, fb_next);
        block_off = 0;
    }

    rc = 0;

done:
    *len = bytes_read;
    ffs_unlock();
    return rc;
}

int
ffs_mkdir(const char *path)
{
    struct ffs_path_parser parser;
    struct ffs_inode *parent;
    struct ffs_inode *inode;
    int rc;

    ffs_lock();

    ffs_path_parser_new(&parser, path);
    rc = ffs_path_find(&parser, &inode, &parent);
    if (rc == 0) {
        rc = FFS_EEXIST;
        goto done;
    }
    if (rc != FFS_ENOENT) {
        goto done;
    }
    if (parser.fpp_token_type != FFS_PATH_TOKEN_LEAF || parent == NULL) {
        rc = FFS_ENOENT;
        goto done;
    }

    rc = ffs_new_file(&inode, parent, parser.fpp_token, 1);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_unlock();
    return rc;
}

void
ffs_free_all(void)
{
    struct ffs_base_list *list;
    struct ffs_inode *inode;
    struct ffs_base *base;
    int i;

    for (i = 0; i < FFS_HASH_SIZE; i++) {
        list = ffs_hash + i;

        base = SLIST_FIRST(list);
        while (base != NULL) {
            if (base->fb_type == FFS_OBJECT_TYPE_INODE) {
                inode = (void *)base;
                while (inode->fi_refcnt > 0) {
                    ffs_inode_dec_refcnt(inode);
                }
                base = SLIST_FIRST(list);
            } else {
                base = SLIST_NEXT(base, fb_hash_next);
            }
        }
    }
}

static int
ffs_validate_scratch(void)
{
    uint32_t scratch_len;
    int i;

    if (ffs_scratch_sector_id == FFS_SECTOR_ID_SCRATCH) {
        /* No scratch sector. */
        return FFS_ECORRUPT;
    }

    scratch_len = ffs_sectors[ffs_scratch_sector_id].fsi_length;
    for (i = 0; i < ffs_num_sectors; i++) {
        if (ffs_sectors[i].fsi_length > scratch_len) {
            return FFS_ECORRUPT;
        }
    }

    return 0;
}

int
ffs_format(void)
{
    int rc;

    ffs_lock();

    rc = ffs_format_full();
    if (rc != 0) {
        goto done;
    }

    rc = ffs_validate_scratch();
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_unlock();
    return rc;
}

static int
ffs_parse_sector_descs(const struct ffs_sector_desc *sector_descs)
{
    int rc;
    int i;

    for (i = 0; sector_descs[i].fsd_length != 0; i++) {
        /* XXX: Validate sector. */
    }
    ffs_num_sectors = i;

    ffs_sectors = malloc(ffs_num_sectors * sizeof *ffs_sectors);
    if (ffs_sectors == NULL) {
        return FFS_ENOMEM;
    }
    for (i = 0; i < ffs_num_sectors; i++) {
        ffs_sectors[i].fsi_offset = sector_descs[i].fsd_offset;
        ffs_sectors[i].fsi_length = sector_descs[i].fsd_length;
        ffs_sectors[i].fsi_cur = 0;
    }

    return 0;
}

static int
ffs_read_disk_object(struct ffs_disk_object *out_disk_object,
                     int sector_id, uint32_t offset)
{
    uint32_t magic;
    int rc;

    rc = ffs_flash_read(sector_id, offset, &magic, sizeof magic);
    if (rc != 0) {
        return rc;
    }

    switch (magic) {
    case FFS_INODE_MAGIC:
        out_disk_object->fdo_type = FFS_OBJECT_TYPE_INODE;
        rc = ffs_inode_read_disk(&out_disk_object->fdo_disk_inode, NULL,
                                 sector_id, offset);
        break;

    case FFS_BLOCK_MAGIC:
        out_disk_object->fdo_type = FFS_OBJECT_TYPE_BLOCK;
        rc = ffs_block_read_disk(&out_disk_object->fdo_disk_block, sector_id,
                                 offset);
        break;

    case 0xffffffff:
        rc = FFS_EEMPTY;
        break;

    default:
        rc = FFS_ECORRUPT;
        break;
    }

    if (rc != 0) {
        return rc;
    }

    out_disk_object->fdo_sector_id = sector_id;
    out_disk_object->fdo_offset = offset;

    return 0;
}

static int
ffs_disk_object_size(const struct ffs_disk_object *disk_object)
{
    switch (disk_object->fdo_type) {
    case FFS_OBJECT_TYPE_INODE:
        return sizeof disk_object->fdo_disk_inode +
                      disk_object->fdo_disk_inode.fdi_filename_len;

    case FFS_OBJECT_TYPE_BLOCK:
        return sizeof disk_object->fdo_disk_block +
                      disk_object->fdo_disk_block.fdb_data_len;

    default:
        assert(0);
        return 1;
    }
}

static int
ffs_detect_one_sector(int sector_id, uint32_t *out_cur)
{
    struct ffs_disk_sector disk_sector;
    struct ffs_disk_object disk_object;
    uint32_t off;
    int rc;

    /* Parse sector header. */
    rc = ffs_flash_read(sector_id, 0, &disk_sector, sizeof disk_sector);
    if (rc != 0) {
        return rc;
    }

    if (disk_sector.fds_magic[0] != FFS_SECTOR_MAGIC0 ||
        disk_sector.fds_magic[1] != FFS_SECTOR_MAGIC1 ||
        disk_sector.fds_magic[2] != FFS_SECTOR_MAGIC2 ||
        disk_sector.fds_magic[3] != FFS_SECTOR_MAGIC3) {

        return FFS_ECORRUPT;
    }

    if (disk_sector.fds_id == FFS_SECTOR_ID_SCRATCH) {
        if (ffs_scratch_sector_id != 0xffff) {
            /* Two scratch sectors. */
            return FFS_ECORRUPT;
        } else {
            ffs_scratch_sector_id = sector_id;
            return 0;
        }
    }

    /* Find end of sector. */
    off = sizeof disk_sector;
    while (1) {
        rc = ffs_read_disk_object(&disk_object, sector_id, off);
        switch (rc) {
        case 0:
            ffs_restore_object(&disk_object);
            off += ffs_disk_object_size(&disk_object);
            break;

        case FFS_EEMPTY:
        case FFS_ERANGE:
            *out_cur = off;
            return 0;

        default:
            return rc;
        }
    }
}

int
ffs_detect_sectors(const struct ffs_sector_desc *sectors)
{
    int rc;
    int i;

    /* XXX: Ensure scratch sector is big enough. */
    /* XXX: Ensure block size is a factor of all sector sizes. */

    ffs_scratch_sector_id = FFS_SECTOR_ID_SCRATCH;

    rc = ffs_parse_sector_descs(sectors);
    if (rc != 0) {
        return rc;
    }

    for (i = 0; i < ffs_num_sectors; i++) {
        rc = ffs_detect_one_sector(i, &ffs_sectors[i].fsi_cur);
        if (rc != 0) {
            return rc;
        }
    }

    rc = ffs_validate_scratch();
    if (rc != 0) {
        return rc;
    }

    ffs_restore_sweep();

    return 0;
}

int
ffs_init(void)
{
    int rc;

    static os_membuf_t file_buf[
        OS_MEMPOOL_SIZE(FFS_NUM_FILES, sizeof (struct ffs_file))];
    static os_membuf_t inode_buf[
        OS_MEMPOOL_SIZE(FFS_NUM_INODES, sizeof (struct ffs_inode))];
    static os_membuf_t block_buf[
        OS_MEMPOOL_SIZE(FFS_NUM_BLOCKS, sizeof (struct ffs_block))];


    ffs_free_all();

    rc = os_mempool_init(&ffs_file_pool, FFS_NUM_FILES,
                         sizeof (struct ffs_file), &file_buf[0],
                         "ffs_file_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    rc = os_mempool_init(&ffs_inode_pool, FFS_NUM_INODES,
                         sizeof (struct ffs_inode), &inode_buf[0],
                         "ffs_inode_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    rc = os_mempool_init(&ffs_block_pool, FFS_NUM_BLOCKS,
                         sizeof (struct ffs_block), &block_buf[0],
                         "ffs_block_pool");
    if (rc != 0) {
        return FFS_EOS;
    }

    rc = os_mutex_create(&ffs_mutex);
    if (rc != 0) {
        return FFS_EOS;
    }

    return 0;
}

