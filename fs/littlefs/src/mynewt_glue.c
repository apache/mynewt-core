/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "os/mynewt.h"
#include <hal/hal_flash.h>
#include <disk/disk.h>
#include <flash_map/flash_map.h>

#include "lfs.h"
#include "lfs_util.h"

#include <fs/fs.h>
#include <fs/fs_if.h>

static int littlefs_open(const char *path, uint8_t access_flags,
                         struct fs_file **out_file);
static int littlefs_close(struct fs_file *fs_file);
static int littlefs_read(struct fs_file *fs_file, uint32_t len, void *out_data,
                         uint32_t *out_len);
static int littlefs_write(struct fs_file *fs_file, const void *data, int len);
static int littlefs_flush(struct fs_file *fs_file);
static int littlefs_seek(struct fs_file *fs_file, uint32_t offset);
static uint32_t littlefs_getpos(const struct fs_file *fs_file);
static int littlefs_file_len(const struct fs_file *fs_file, uint32_t *out_len);
static int littlefs_unlink(const char *path);
static int littlefs_rename(const char *from, const char *to);
static int littlefs_mkdir(const char *path);
static int littlefs_opendir(const char *path, struct fs_dir **out_fs_dir);
static int littlefs_readdir(struct fs_dir *dir, struct fs_dirent **out_dirent);
static int littlefs_closedir(struct fs_dir *dir);
static int littlefs_dirent_name(const struct fs_dirent *fs_dirent, size_t max_len,
                                char *out_name, uint8_t *out_name_len);
static int littlefs_dirent_is_dir(const struct fs_dirent *fs_dirent);

struct littlefs_file {
    struct fs_ops *fops;
    lfs_file_t *file;
    lfs_t *lfs;
};

struct littlefs_dirent {
    struct fs_ops *fops;
    struct lfs_info info;
};

struct littlefs_dir {
    struct fs_ops *fops;
    lfs_dir_t *dir;
    struct littlefs_dirent *cur_dirent;
    lfs_t *lfs;
};

static struct fs_ops littlefs_ops = {
    .f_open = littlefs_open,
    .f_close = littlefs_close,
    .f_read = littlefs_read,
    .f_write = littlefs_write,
    .f_flush = littlefs_flush,

    .f_seek = littlefs_seek,
    .f_getpos = littlefs_getpos,
    .f_filelen = littlefs_file_len,

    .f_unlink = littlefs_unlink,
    .f_rename = littlefs_rename,
    .f_mkdir = littlefs_mkdir,

    .f_opendir = littlefs_opendir,
    .f_readdir = littlefs_readdir,
    .f_closedir = littlefs_closedir,

    .f_dirent_name = littlefs_dirent_name,
    .f_dirent_is_dir = littlefs_dirent_is_dir,

    .f_name = "littlefs"
};

static int
littlefs_to_vfs_error(int err)
{
    int rc = FS_EOS;

    switch (err) {
    case LFS_ERR_OK:
        rc = FS_EOK;
        break;
    case LFS_ERR_IO:
        rc = FS_EHW;
        break;
    case LFS_ERR_EXIST:
        rc = FS_EEXIST;
        break;
    case LFS_ERR_NOENT:
        rc = FS_ENOENT;
        break;
    case LFS_ERR_NOSPC:
        rc = FS_EFULL;
        break;
    case LFS_ERR_CORRUPT:
        rc = FS_ECORRUPT;
        break;
    case LFS_ERR_NOMEM:
        rc = FS_ENOMEM;
        break;
    case LFS_ERR_INVAL:           /* fallthrough */
    case LFS_ERR_BADF:            /* fallthrough */
    case LFS_ERR_FBIG:            /* fallthrough */
    case LFS_ERR_NOTEMPTY:        /* fallthrough */
    case LFS_ERR_NOTDIR:          /* fallthrough */
    case LFS_ERR_ISDIR:           /* fallthrough */
        rc = FS_EINVAL;
        break;
    default:
        /* Possibly unhandled error, maybe in newer versions */
        assert(0);
    }

    return rc;
}

/*
 * Read a region in a block. Negative error codes are propagated
 * to the user.
 */
static int
flash_read(const struct lfs_config *c, lfs_block_t block,
           lfs_off_t off, void *buffer, lfs_size_t size)
{
    int rc;
    const struct flash_area *fa;
    uint32_t offset;

    fa = (const struct flash_area *)(c->context);
    offset = c->block_size * block + off;
    rc = flash_area_read(fa, offset, buffer, size);
    if (rc != 0) {
        return LFS_ERR_IO;
    }

    return 0;
}

/*
 * Program a region in a block. The block must have previously
 * been erased. Negative error codes are propogated to the user.
 * May return LFS_ERR_CORRUPT if the block should be considered bad.
 */
static int
flash_prog(const struct lfs_config *c, lfs_block_t block,
           lfs_off_t off, const void *buffer, lfs_size_t size)
{
    int rc;
    const struct flash_area *fa;
    uint32_t offset;

    fa = (const struct flash_area *)(c->context);
    offset = c->block_size * block + off;
    rc = flash_area_write(fa, offset, buffer, (uint32_t)size);
    if (rc != 0) {
        return LFS_ERR_IO;
    }

    return 0;
}

/*
 * Erase a block. A block must be erased before being programmed.
 * The state of an erased block is undefined. Negative error codes
 * are propogated to the user.
 * May return LFS_ERR_CORRUPT if the block should be considered bad.
 */
static int
flash_erase(const struct lfs_config *c, lfs_block_t block)
{
    int rc;
    const struct flash_area *fa;

    fa = (const struct flash_area *)(c->context);
    rc = flash_area_erase(fa, c->block_size * block, c->block_size);
    if (rc != 0) {
        return LFS_ERR_IO;
    }

    return 0;
}

/*
 * Sync the state of the underlying block device. Negative error codes
 * are propogated to the user.
 */
static int
flash_sync(const struct lfs_config *c)
{
    /* all operations are synchronous already */
    return 0;
}

static lfs_t *g_lfs = NULL;
static bool g_lfs_alloc_done = false;

#define READ_SIZE       MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE)
#define PROG_SIZE       MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE)
#define CACHE_SIZE      16
#define LOOKAHEAD_SIZE  16

static uint8_t read_buffer[CACHE_SIZE];
static uint8_t prog_buffer[CACHE_SIZE];
static uint8_t __attribute__((aligned(4))) lookahead_buffer[LOOKAHEAD_SIZE];
static struct lfs_config g_lfs_cfg = {
    .context = NULL,

    .read = flash_read,
    .prog = flash_prog,
    .erase = flash_erase,
    .sync = flash_sync,

    /* block device configuration */
    .read_size = READ_SIZE,
    .prog_size = PROG_SIZE,
    .block_size = MYNEWT_VAL(LITTLEFS_BLOCK_SIZE),
    .block_count = MYNEWT_VAL(LITTLEFS_BLOCK_COUNT),
    .block_cycles = 500,
    .cache_size = CACHE_SIZE,
    .lookahead_size = LOOKAHEAD_SIZE,
    .read_buffer = read_buffer,
    .prog_buffer = prog_buffer,
    .lookahead_buffer = lookahead_buffer,
    .name_max = 0,
    .file_max = 0,
    .attr_max = 0,
    .metadata_max = 0,
};

static struct os_mutex littlefs_mutex;

static void
littlefs_lock(void)
{
    int rc;

    rc = os_mutex_pend(&littlefs_mutex, OS_TIMEOUT_NEVER);
    assert(rc == 0 || rc == OS_NOT_STARTED);
}

static void
littlefs_unlock(void)
{
    int rc;

    rc = os_mutex_release(&littlefs_mutex);
    assert(rc == 0 || rc == OS_NOT_STARTED);
}

static int
littlefs_open(const char *path, uint8_t access_flags, struct fs_file **out_fs_file)
{
    lfs_file_t *out_file = NULL;
    struct littlefs_file *file = NULL;
    int flags;
    int rc;

    if (!path || !out_fs_file) {
        return FS_EINVAL;
    }

    out_file = NULL;

    file = malloc(sizeof(struct littlefs_file));
    if (!file) {
        rc = FS_ENOMEM;
        goto out;
    }

    out_file = malloc(sizeof(lfs_file_t));
    if (!out_file) {
        rc = FS_ENOMEM;
        goto out;
    }

    /*
     * TODO: LitteFS also has LFS_O_EXCL, which causes a failure if a file
     * already exists. This is currently not supported in the FS abstraction.
     */

    flags = 0;
    if ((access_flags & (FS_ACCESS_READ | FS_ACCESS_WRITE)) ==
        (FS_ACCESS_READ | FS_ACCESS_WRITE)) {
        flags |= LFS_O_RDWR;
    } else if (access_flags & FS_ACCESS_READ) {
        flags |= LFS_O_RDONLY;
    } else if (access_flags & FS_ACCESS_WRITE) {
        flags |= LFS_O_WRONLY;
    }

    flags |= LFS_O_CREAT;

    if (access_flags & FS_ACCESS_APPEND) {
        flags |= LFS_O_APPEND;
    }
    if (access_flags & FS_ACCESS_TRUNCATE) {
        flags |= LFS_O_TRUNC;
    }

    littlefs_lock();
    rc = lfs_file_open(g_lfs, out_file, path, flags);
    littlefs_unlock();
    if (rc != LFS_ERR_OK) {
        rc = littlefs_to_vfs_error(rc);
        goto out;
    }

    file->file = out_file;
    file->fops = &littlefs_ops;
    file->lfs = g_lfs;
    *out_fs_file = (struct fs_file *) file;
    rc = FS_EOK;

out:
    if (rc != FS_EOK) {
        free(file);
        free(out_file);
    }
    return rc;
}

static int
littlefs_close(struct fs_file *fs_file)
{
    int rc;
    lfs_file_t *file;
    lfs_t *lfs;

    if (!fs_file) {
        return FS_EINVAL;
    }

    file = ((struct littlefs_file *) fs_file)->file;
    if (!file) {
        return FS_EOK;
    }

    lfs = ((struct littlefs_file *) fs_file)->lfs;

    littlefs_lock();
    rc = lfs_file_close(lfs, file);
    littlefs_unlock();
    free(file);

    return littlefs_to_vfs_error(rc);
}

static int
littlefs_seek(struct fs_file *fs_file, uint32_t offset)
{
    lfs_file_t *file;
    lfs_t *lfs;
    int rc;

    if (!fs_file) {
        return FS_EINVAL;
    }

    file = ((struct littlefs_file *) fs_file)->file;
    lfs = ((struct littlefs_file *) fs_file)->lfs;

    /* Returns the new position if succesful */
    littlefs_lock();
    rc = lfs_file_seek(lfs, file, offset, LFS_SEEK_SET);
    littlefs_unlock();
    if (rc < 0) {
        return littlefs_to_vfs_error(rc);
    }

    return FS_EOK;
}

static uint32_t
littlefs_getpos(const struct fs_file *fs_file)
{
    lfs_file_t *file;
    lfs_t *lfs;
    int rc;

    if (!fs_file) {
        return FS_EINVAL;
    }

    file = ((struct littlefs_file *) fs_file)->file;
    lfs = ((struct littlefs_file *) fs_file)->lfs;

    /*
     * LttleFS can return < 0 on errors, but fs_getpos does not allow
     * failing, so just return 0 and hope for the best. This should
     * eventually be fixed in the FS abstraction.
     */
    littlefs_lock();
    rc = lfs_file_tell(lfs, file);
    littlefs_unlock();
    if (rc < 0) {
        return 0;
    }

    return (uint32_t)rc;
}

static int
littlefs_file_len(const struct fs_file *fs_file, uint32_t *out_len)
{
    lfs_file_t *file;
    lfs_t *lfs;
    int32_t len;

    if (!fs_file || !out_len) {
        return FS_EINVAL;
    }

    file = ((struct littlefs_file *) fs_file)->file;
    lfs = ((struct littlefs_file *) fs_file)->lfs;

    littlefs_lock();
    len = (int32_t)lfs_file_size(lfs, file);
    littlefs_unlock();
    if (len < 0) {
        return littlefs_to_vfs_error((int)len);
    }

    *out_len = (uint32_t)len;
    return FS_EOK;
}

static int
littlefs_read(struct fs_file *fs_file, uint32_t len, void *out_data,
              uint32_t *out_len)
{
    lfs_file_t *file;
    lfs_t *lfs;
    int32_t size;

    if (!fs_file || !out_data || !out_len) {
        return FS_EINVAL;
    }

    if (!len) {
        *out_len = 0;
        return FS_EOK;
    }

    file = ((struct littlefs_file *) fs_file)->file;
    lfs = ((struct littlefs_file *) fs_file)->lfs;

    littlefs_lock();
    size = lfs_file_read(lfs, file, out_data, len);
    littlefs_unlock();
    if (size < 0) {
        return littlefs_to_vfs_error((int)size);
    }

    *out_len = (uint32_t)size;
    return FS_EOK;
}

static int
littlefs_write(struct fs_file *fs_file, const void *data, int len)
{
    lfs_file_t *file;
    lfs_t *lfs;
    int32_t size;

    if (!fs_file || !data) {
        return FS_EINVAL;
    }

    if (!len) {
        return FS_EOK;
    }

    file = ((struct littlefs_file *) fs_file)->file;
    lfs = ((struct littlefs_file *) fs_file)->lfs;

    littlefs_lock();
    size = lfs_file_write(lfs, file, data, len);
    littlefs_unlock();
    if (size < 0) {
        return littlefs_to_vfs_error((int)size);
    }

    if (size != len) {
        return FS_EFULL;
    }

    return FS_EOK;
}

static int
littlefs_flush(struct fs_file *fs_file)
{
    (void)fs_file;
    return FS_EOK;
}

static int
littlefs_unlink(const char *path)
{
    int rc;

    if (!path) {
        return FS_EINVAL;
    }

    littlefs_lock();
    rc = lfs_remove(g_lfs, path);
    littlefs_unlock();

    return littlefs_to_vfs_error(rc);
}

static int
littlefs_rename(const char *from, const char *to)
{
    int rc;

    if (!from || !to) {
        return FS_EINVAL;
    }

    littlefs_lock();
    rc = lfs_rename(g_lfs, from, to);
    littlefs_unlock();

    return littlefs_to_vfs_error(rc);
}

static int
littlefs_mkdir(const char *path)
{
    int rc;

    if (!path) {
        return FS_EINVAL;
    }

    littlefs_lock();
    rc = lfs_mkdir(g_lfs, path);
    littlefs_unlock();

    return littlefs_to_vfs_error(rc);
}

static int
littlefs_opendir(const char *path, struct fs_dir **out_fs_dir)
{
    lfs_dir_t *out_dir = NULL;
    struct littlefs_dir *dir = NULL;
    int rc;

    if (!path || !out_fs_dir) {
        return FS_EINVAL;
    }

    out_dir = NULL;

    dir = malloc(sizeof(struct littlefs_dir));
    if (!dir) {
        rc = FS_ENOMEM;
        goto out;
    }

    out_dir = malloc(sizeof(lfs_dir_t));
    if (!out_dir) {
        rc = FS_ENOMEM;
        goto out;
    }

    littlefs_lock();
    rc = lfs_dir_open(g_lfs, out_dir, path);
    littlefs_unlock();
    if (rc < 0) {
        rc = littlefs_to_vfs_error(rc);
        goto out;
    }

    dir->dir = out_dir;
    dir->cur_dirent = NULL;
    dir->fops = &littlefs_ops;
    dir->lfs = g_lfs;
    *out_fs_dir = (struct fs_dir *)dir;
    rc = FS_EOK;

out:
    if (rc != FS_EOK) {
        free(dir);
        free(out_dir);
    }
    return rc;
}

static int
littlefs_readdir(struct fs_dir *fs_dir, struct fs_dirent **out_fs_dirent)
{
    int rc;
    lfs_dir_t *dir;
    struct littlefs_dir *ldir;
    lfs_t *lfs;
    struct littlefs_dirent *dirent;

    if (!fs_dir || !out_fs_dirent) {
        return FS_EINVAL;
    }

    ldir = (struct littlefs_dir *)fs_dir;
    if (!ldir->cur_dirent) {
        dirent = malloc(sizeof(struct littlefs_dirent));
        if (!dirent) {
            return FS_ENOMEM;
        }
        ldir->cur_dirent = dirent;
        ldir->cur_dirent->fops = ldir->fops;
    }

    dirent = ldir->cur_dirent;
    dir = ldir->dir;
    lfs = ldir->lfs;

    littlefs_lock();
    rc = lfs_dir_read(lfs, dir, &dirent->info);
    littlefs_unlock();
    if (rc < 0) {
        free(dirent);
        ldir->cur_dirent = NULL;
        return littlefs_to_vfs_error(rc);
    }

    if (rc == 0) {
        free(dirent);
        ldir->cur_dirent = NULL;
        return FS_ENOENT;
    }

    *out_fs_dirent = (struct fs_dirent *)dirent;
    return FS_EOK;
}

static int
littlefs_closedir(struct fs_dir *fs_dir)
{
    int rc;
    lfs_dir_t *dir;
    lfs_t *lfs;

    if (!fs_dir) {
        return FS_EINVAL;
    }

    dir = ((struct littlefs_dir *) fs_dir)->dir;
    lfs = ((struct littlefs_dir *) fs_dir)->lfs;

    littlefs_lock();
    rc = lfs_dir_close(lfs, dir);
    littlefs_unlock();

    free(((struct littlefs_dir *) fs_dir)->cur_dirent);
    free(dir);
    return littlefs_to_vfs_error(rc);
}

static int
littlefs_dirent_name(const struct fs_dirent *fs_dirent, size_t max_len,
                     char *out_name, uint8_t *out_name_len)
{

    struct lfs_info *info;
    size_t len;

    if (!fs_dirent || !out_name || !out_name_len) {
        return FS_EINVAL;
    }

    if (!max_len) {
        *out_name_len = 0;
        return FS_EOK;
    }

    info = &((struct littlefs_dirent *)fs_dirent)->info;
    len = min(max_len, strlen(info->name) + 1) - 1;
    memcpy(out_name, info->name, len);
    out_name[len] = '\0';
    *out_name_len = len;  /* Don't include ending NULL */
    return FS_EOK;
}

static int
littlefs_dirent_is_dir(const struct fs_dirent *fs_dirent)
{
    struct lfs_info *info;

    if (!fs_dirent) {
        return FS_EINVAL;
    }

    info = &((struct littlefs_dirent *)fs_dirent)->info;
    return info->type == LFS_TYPE_DIR;
}

/*
 * Initializes only Mynewt glue, to fully initialize call
 * LitteFS must call littlefs_format or littlefs_mount.
 */
static int
littlefs_alloc(void)
{
    const struct flash_area *fa;
    int rc;

    /*
     * Already initialized.
     */
    if (g_lfs && g_lfs_alloc_done) {
        return FS_EOK;
    }

    rc = os_mutex_init(&littlefs_mutex);
    if (rc != 0) {
        return FS_EOS;
    }

    g_lfs = malloc(sizeof(lfs_t));
    if (!g_lfs) {
        return FS_ENOMEM;
    }

    /*
     * This doesn't seem to be needed because lfs_mount initializes
     * all fields, but just to stay on the safe side...
     */
    memset(g_lfs, 0, sizeof(lfs_t));

    rc = flash_area_open(MYNEWT_VAL(LITTLEFS_FLASH_AREA), &fa);
    if (rc) {
        free(g_lfs);
        g_lfs = NULL;
        return FS_EHW;
    }

    /*
     * TODO: could check that fa_size matches the configured block size * count
     */
    g_lfs_cfg.context = (struct flash_area *)fa;
    g_lfs_alloc_done = true;

    return FS_EOK;
}

int
littlefs_reformat(void)
{
    int rc;

    if (!g_lfs_alloc_done) {
        rc = littlefs_alloc();
        if (rc != FS_EOK) {
            return -1;
        }
    }

    return lfs_format(g_lfs, &g_lfs_cfg);
}

int
littlefs_init(void)
{
    int rc;

    if (!g_lfs_alloc_done) {
        rc = littlefs_alloc();
        if (rc != FS_EOK) {
            return rc;
        }
    }

    rc = lfs_mount(g_lfs, &g_lfs_cfg);
    switch (rc) {
    case LFS_ERR_OK:
        break;
    case LFS_ERR_INVAL:
    case LFS_ERR_CORRUPT:
        /* No valid LittleFS instance detected; act based on configued
         * detection failure policy.
         */
#if MYNEWT_VAL(LITTLEFS_DETECT_FAIL_FORMAT)
        rc = lfs_format(g_lfs, &g_lfs_cfg);
        if (!rc) {
            rc = lfs_mount(g_lfs, &g_lfs_cfg);
        }
#endif
        break;
    }

    if (!rc) {
        fs_register(&littlefs_ops);
    }

    return rc;
}

void
littlefs_pkg_init(void)
{
#if !MYNEWT_VAL(LITTLEFS_DISABLE_SYSINIT)
    int rc;

    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    /* Attempt to restore an existing littlefs file system from flash. */
    rc = littlefs_init();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
