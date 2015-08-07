#include <assert.h>
#include <string.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"

/**
 * Creates a new empty file and writes it to the file system.  If a file with
 * the specified path already exists, the behavior is undefined.
 *
 * @param out_inode_entry       On success, this points to the inode
 *                                  corresponding to the new file.
 * @param parent                The parent directory to insert the new file in.
 * @param filename              The name of the file to create.
 * @param filename_len          The length of the filename, in characters.
 * @param is_dir                1 if this is a directory; 0 if it is a normal
 *                                  file.
 *
 * @return                      0 on success; nonzero on failure.
 */
int
ffs_file_new(struct ffs_inode_entry **out_inode_entry,
             struct ffs_inode_entry *parent,
             const char *filename, uint8_t filename_len, int is_dir)
{
    struct ffs_disk_inode disk_inode;
    struct ffs_inode_entry *inode_entry;
    uint32_t offset;
    uint8_t area_idx;
    int rc;

    inode_entry = ffs_inode_entry_alloc();
    if (inode_entry == NULL) {
        rc = FFS_ENOMEM;
        goto err;
    }

    rc = ffs_misc_reserve_space(&area_idx, &offset,
                                sizeof disk_inode + filename_len);
    if (rc != 0) {
        goto err;
    }

    memset(&disk_inode, 0xff, sizeof disk_inode);
    disk_inode.fdi_magic = FFS_INODE_MAGIC;
    if (is_dir) {
        disk_inode.fdi_id = ffs_hash_next_dir_id++;
    } else {
        disk_inode.fdi_id = ffs_hash_next_file_id++;
    }
    disk_inode.fdi_seq = 0;
    if (parent == NULL) {
        disk_inode.fdi_parent_id = FFS_ID_NONE;
    } else {
        disk_inode.fdi_parent_id = parent->fi_hash_entry.fhe_id;
    }
    disk_inode.fdi_filename_len = filename_len;

    rc = ffs_inode_write_disk(&disk_inode, filename, area_idx, offset);
    if (rc != 0) {
        goto err;
    }

    inode_entry->fi_hash_entry.fhe_id = disk_inode.fdi_id;
    inode_entry->fi_hash_entry.fhe_flash_loc = ffs_flash_loc(area_idx, offset);
    inode_entry->fi_refcnt = 1;

    if (parent != NULL) {
        rc = ffs_inode_add_child(parent, inode_entry);
        if (rc != 0) {
            goto err;
        }
    } else {
        assert(disk_inode.fdi_id == FFS_ID_ROOT_DIR);
    }

    ffs_hash_insert(&inode_entry->fi_hash_entry);
    *out_inode_entry = inode_entry;

    return 0;

err:
    ffs_inode_entry_free(inode_entry);
    return rc;
}

/**
 * Performs a file open operation.
 *
 * @param out_file          On success, a pointer to the newly-created file
 *                              handle gets written here.
 * @param path              The path of the file to open.
 * @param access_flags      Flags controlling file access; see ffs_open() for
 *                              details.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
ffs_file_open(struct ffs_file **out_file, const char *path,
              uint8_t access_flags)
{
    struct ffs_path_parser parser;
    struct ffs_inode_entry *parent;
    struct ffs_inode_entry *inode;
    struct ffs_file *file;
    int rc;

    file = NULL;

    /* Reject invalid access flag combinations. */
    if (!(access_flags & (FFS_ACCESS_READ | FFS_ACCESS_WRITE))) {
        rc = FFS_EINVAL;
        goto err;
    }
    if (access_flags & (FFS_ACCESS_APPEND | FFS_ACCESS_TRUNCATE) &&
        !(access_flags & FFS_ACCESS_WRITE)) {

        rc = FFS_EINVAL;
        goto err;
    }
    if (access_flags & FFS_ACCESS_APPEND &&
        access_flags & FFS_ACCESS_TRUNCATE) {

        rc = FFS_EINVAL;
        goto err;
    }

    file = os_memblock_get(&ffs_file_pool);
    if (file == NULL) {
        rc = FFS_ENOMEM;
        goto err;
    }

    ffs_path_parser_new(&parser, path);
    rc = ffs_path_find(&parser, &inode, &parent);
    if (rc == FFS_ENOENT) {
        /* The file does not exist.  This is fatal for read-only opens. */
        if (!(access_flags & FFS_ACCESS_WRITE)) {
            goto err;
        }

        /* Make sure the parent directory exists. */
        if (parent == NULL) {
            goto err;
        }

        /* Create a new file at the specified path. */
        rc = ffs_file_new(&file->ff_inode_entry, parent, parser.fpp_token,
                          parser.fpp_token_len, 0);
        if (rc != 0) {
            goto err;
        }
    } else {
        /* The file already exists. */

        /* Reject an attempt to open a directory. */
        if (parser.fpp_token_type != FFS_PATH_TOKEN_LEAF) {
            rc = FFS_EINVAL;
            goto err;
        }

        if (access_flags & FFS_ACCESS_TRUNCATE) {
            /* The user is truncating the file.  Unlink the old file and create
             * a new one in its place.
             */
            ffs_path_unlink(path);
            rc = ffs_file_new(&file->ff_inode_entry, parent, parser.fpp_token,
                              parser.fpp_token_len, 0);
            if (rc != 0) {
                goto err;
            }
        } else {
            /* The user is not truncating the file.  Point the file handle to
             * the existing inode.
             */
            file->ff_inode_entry = inode;
        }
    }

    if (access_flags & FFS_ACCESS_APPEND) {
        rc = ffs_inode_calc_data_length(&file->ff_offset,
                                        file->ff_inode_entry);
        if (rc != 0) {
            goto err;
        }
    } else {
        file->ff_offset = 0;
    }
    file->ff_inode_entry->fi_refcnt++;
    file->ff_access_flags = access_flags;

    *out_file = file;

    return 0;

err:
    os_memblock_put(&ffs_file_pool, file);
    return rc;
}

/**
 * Positions a file's read and write pointer at the specified offset.  The
 * offset is expressed as the number of bytes from the start of the file (i.e.,
 * seeking to 0 places the pointer at the first byte in the file).
 *
 * @param file              The file to reposition.
 * @param offset            The offset from the start of the file to seek to.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
ffs_file_seek(struct ffs_file *file, uint32_t offset)
{ 
    uint32_t len;
    int rc;

    rc = ffs_inode_calc_data_length(&len, file->ff_inode_entry);
    if (rc != 0) {
        return rc;
    }

    if (offset > len) {
        return FFS_ERANGE;
    }

    file->ff_offset = offset;
    return 0;
}

/**
 * Closes the specified file and invalidates the file handle.  If the file has
 * already been unlinked, and this is the last open handle to the file, this
 * operation causes the file to be deleted.
 *
 * @param file              The file handle to close.
 *
 * @return                  0 on success; nonzero on failure.
 */
int
ffs_file_close(struct ffs_file *file)
{
    int rc;

    rc = ffs_inode_dec_refcnt(NULL, file->ff_inode_entry);
    if (rc != 0) {
        return rc;
    }

    rc = os_memblock_put(&ffs_file_pool, file);
    if (rc != 0) {
        return FFS_EOS;
    }

    return 0;
}
