#include <assert.h>
#include <string.h>
#include "ffs_priv.h"
#include "ffs/ffs.h"

int
ffs_file_new(struct ffs_inode **out_inode, struct ffs_inode *parent,
             const char *filename, uint8_t filename_len, int is_dir)
{
    struct ffs_disk_inode disk_inode;
    struct ffs_inode *inode;
    uint16_t area_id;
    uint32_t offset;
    int rc;

    inode = ffs_inode_alloc();
    if (inode == NULL) {
        rc = FFS_ENOMEM;
        goto err;
    }

    rc = ffs_misc_reserve_space(&area_id, &offset,
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

    rc = ffs_inode_write_disk(&disk_inode, filename, area_id, offset);
    if (rc != 0) {
        goto err;
    }

    rc = ffs_inode_from_disk(inode, &disk_inode, area_id, offset);
    if (rc != 0) {
        goto err;
    }
    if (parent != NULL) {
        rc = ffs_inode_add_child(parent, inode);
        if (rc != 0) {
            goto err;
        }
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
ffs_file_open(struct ffs_file **out_file, const char *filename,
              uint8_t access_flags)
{
    struct ffs_path_parser parser;
    struct ffs_inode *parent;
    struct ffs_inode *inode;
    struct ffs_file *file;
    int rc;

    file = NULL;

    /* XXX: Validate arguments. */
    if (!(access_flags & (FFS_ACCESS_READ | FFS_ACCESS_WRITE))) {
        rc = FFS_EINVAL;
        goto err;
    }
    if (access_flags & FFS_ACCESS_APPEND &&
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

    ffs_path_parser_new(&parser, filename);
    rc = ffs_path_find(&parser, &inode, &parent);
    if (rc == FFS_ENOENT) {
        if (parent == NULL || !(access_flags & FFS_ACCESS_WRITE)) {
            goto err;
        }

        assert(parser.fpp_token_type == FFS_PATH_TOKEN_LEAF); // XXX
        rc = ffs_file_new(&file->ff_inode, parent, parser.fpp_token,
                          parser.fpp_token_len, 0);
        if (rc != 0) {
            goto err;
        }
    } else if (access_flags & FFS_ACCESS_TRUNCATE) {
        ffs_path_unlink(filename);
        rc = ffs_file_new(&file->ff_inode, parent, parser.fpp_token,
                          parser.fpp_token_len, 0);
        if (rc != 0) {
            goto err;
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

    return 0;

err:
    os_memblock_put(&ffs_file_pool, file);
    return rc;
}

int
ffs_file_seek(struct ffs_file *file, uint32_t offset)
{ 
    if (offset > file->ff_inode->fi_data_len) {
        return FFS_ERANGE;
    }

    file->ff_offset = offset;
    return 0;
}

int
ffs_file_close(struct ffs_file *file)
{
    int rc;

    ffs_inode_dec_refcnt(file->ff_inode);

    rc = os_memblock_put(&ffs_file_pool, file);
    if (rc != 0) {
        return FFS_EOS;
    }

    return 0;
}
