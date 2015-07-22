#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "hal/hal_flash.h"
#include "os/os_mempool.h"
#include "os/os_mutex.h"
#include "ffs_priv.h"
#include "ffs/ffs.h"

#define FFS_NUM_FILES           8
#define FFS_NUM_INODES          100
#define FFS_NUM_BLOCKS          100

#define FFS_MAX_SECTORS         32 /* XXX: Temporary. */

struct ffs_sector_info ffs_sector_array[FFS_MAX_SECTORS];
struct ffs_sector_info *ffs_sectors = ffs_sector_array;
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
             const char *filename, uint8_t filename_len, int is_dir)
{
    struct ffs_disk_inode disk_inode;
    struct ffs_inode *inode;
    uint16_t sector_id;
    uint32_t offset;
    int rc;

    inode = ffs_inode_alloc();
    if (inode == NULL) {
        rc = FFS_ENOMEM;
        goto err;
    }

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
        rc = ffs_new_file(&file->ff_inode, parent, parser.fpp_token,
                          parser.fpp_token_len, 0);
        if (rc != 0) {
            goto done;
        }
    } else if (access_flags & FFS_ACCESS_TRUNCATE) {
        ffs_unlink_internal(filename);
        rc = ffs_new_file(&file->ff_inode, parent, parser.fpp_token,
                          parser.fpp_token_len, 0);
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

int
ffs_read(struct ffs_file *file, void *data, uint32_t *len)
{
    int rc;

    ffs_lock();

    rc = ffs_inode_read(file->ff_inode, file->ff_offset, data, len);
    if (rc != 0) {
        goto done;
    }

    file->ff_offset += *len;

    rc = 0;

done:
    ffs_unlock();
    return rc;
}

/**
 * Writes the supplied data to the specified file handle.
 *
 * @param file              The file to write to.
 * @param data              The data to write.
 * @param len               The number of bytes to write.
 *
 * @return                  0 on success;
 *                          nonzero on failure.
 */
int
ffs_write(struct ffs_file *file, const void *data, int len)
{
    int rc;

    ffs_lock();
    rc = ffs_write_to_file(file, data, len);
    ffs_unlock();

    return rc;
}

/**
 * Creates the directory represented by the specified path.  All intermediate
 * directories must already exist.  The specified path must start with a '/'
 * character.
 *
 * @param path                  The directory to create.
 *
 * @return                      0 on success;
 *                              nonzero on failure.
 */
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

    rc = ffs_new_file(&inode, parent, parser.fpp_token, parser.fpp_token_len,
                      1);
    if (rc != 0) {
        goto done;
    }

    rc = 0;

done:
    ffs_unlock();
    return rc;
}

/**
 * Erases all the specified sectors and initializes them with a clean ffs
 * file system.
 *
 * @param sector_descs      The set of sectors to format.
 *
 * @return                  0 on success;
 *                          nonzero on failure.
 */
int
ffs_format(const struct ffs_sector_desc *sector_descs)
{
    int rc;

    ffs_lock();
    rc = ffs_format_full(sector_descs);
    ffs_unlock();

    return rc;
}

/**
 * Searches for a valid ffs file system among the specified sectors.  This
 * function succeeds if a file system is detected among any subset of the
 * supplied sectors.  If the sector set does not contain a valid file system,
 * a new one can be created via a call to ffs_format().
 *
 * @param sector_descs      The sector set to search.  This array must be
 *                          terminated with a 0-length sector.
 *
 * @return                  0 on success;
 *                          FFS_ECORRUPT if no valid file system was detected;
 *                          other nonzero on error.
 */
int
ffs_detect(const struct ffs_sector_desc *sector_descs)
{
    int rc;

    ffs_lock();
    rc = ffs_restore_full(sector_descs);
    ffs_unlock();

    return rc;
}

/**
 * Initializes the ffs memory and data structures.  This must be called before
 * any ffs operations are attempted.
 *
 * @return                  0 on success; nonzero on error.
 */
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


    ffs_format_ram();

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

