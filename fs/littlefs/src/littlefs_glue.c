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
#include <os/mynewt.h>
#include <hal/hal_flash.h>
#include <flash_map/flash_map.h>
#include <fs/fs.h>
#include <fs/fs_if.h>
#include "lfs.h"
#include "lfs_util.h"

static int flash_read(const struct lfs_config *c, lfs_block_t block,
                      lfs_off_t off, void *buffer, lfs_size_t size);
static int flash_prog(const struct lfs_config *c, lfs_block_t block,
                      lfs_off_t off, const void *buffer, lfs_size_t size);
static int flash_erase(const struct lfs_config *c, lfs_block_t block);
static int flash_sync(const struct lfs_config *c);

#ifdef LFS_THREADSAFE
static int littlefs_lock(const struct lfs_config *c);
static int littlefs_unlock(const struct lfs_config *c);
#endif

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

/* Min block size equired by littlefs implementation */
#define MIN_BLOCK_SIZE 128

/* Use min block size for cache and lookahead (no reason, just an arbitrary value) */
#define CACHE_SIZE     MIN_BLOCK_SIZE
#define LOOKAHEAD_SIZE MIN_BLOCK_SIZE

static uint32_t g_littlefs_read_buf[CACHE_SIZE / sizeof(uint32_t)];
static uint32_t g_littlefs_prog_buf[CACHE_SIZE / sizeof(uint32_t)];
static uint32_t g_littlefs_lookahead_buf[LOOKAHEAD_SIZE / sizeof(uint32_t)];

#ifdef LFS_THREADSAFE
static struct os_mutex g_littlefs_mutex;
#endif

static lfs_t g_littlefs;
static struct lfs_config g_littlefs_config = {
    .context = NULL,

    .read = flash_read,
    .prog = flash_prog,
    .erase = flash_erase,
    .sync = flash_sync,

#ifdef LFS_THREADSAFE
    .lock = littlefs_lock,
    .unlock = littlefs_unlock,
#endif

    .read_size = MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE),
    .prog_size = MYNEWT_VAL(MCU_FLASH_MIN_WRITE_SIZE),
    .block_size = MYNEWT_VAL(LITTLEFS_BLOCK_SIZE),
    .block_count = MYNEWT_VAL(LITTLEFS_BLOCK_COUNT),
    .block_cycles = MYNEWT_VAL(LITTLEFS_BLOCK_CYCLES),
    .cache_size = CACHE_SIZE,
    .lookahead_size = LOOKAHEAD_SIZE,
    .read_buffer = g_littlefs_read_buf,
    .prog_buffer = g_littlefs_prog_buf,
    .lookahead_buffer = g_littlefs_lookahead_buf,
    .name_max = 0,
    .file_max = 0,
    .attr_max = 0,
    .metadata_max = 0,
    .inline_max = MYNEWT_VAL(LITTLEFS_DISABLE_INLINED_FILES) ? -1 : 0,
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
    default:
        rc = FS_EINVAL;
        break;
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

#ifdef LFS_THREADSAFE
static int
littlefs_lock(const struct lfs_config *c)
{
    int rc;

    rc = os_mutex_pend(&g_littlefs_mutex, OS_TIMEOUT_NEVER);

    return (rc == 0 || rc == OS_NOT_STARTED) ? LFS_ERR_OK : LFS_ERR_IO;
}

static int
littlefs_unlock(const struct lfs_config *c)
{
    int rc;

    rc = os_mutex_release(&g_littlefs_mutex);

    return (rc == 0 || rc == OS_NOT_STARTED) ? LFS_ERR_OK : LFS_ERR_IO;
}
#endif

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
        flags = LFS_O_RDWR | LFS_O_CREAT;
    } else if (access_flags & FS_ACCESS_READ) {
        flags = LFS_O_RDONLY;
    } else if (access_flags & FS_ACCESS_WRITE) {
        flags = LFS_O_WRONLY | LFS_O_CREAT;
    }
    if (access_flags & FS_ACCESS_APPEND) {
        flags |= LFS_O_APPEND;
    }
    if (access_flags & FS_ACCESS_TRUNCATE) {
        flags |= LFS_O_TRUNC;
    }

    rc = lfs_file_open(&g_littlefs, out_file, path, flags);
    if (rc != LFS_ERR_OK) {
        rc = littlefs_to_vfs_error(rc);
        goto out;
    }

    file->file = out_file;
    file->fops = &littlefs_ops;
    file->lfs = &g_littlefs;
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

    rc = lfs_file_close(lfs, file);
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
    rc = lfs_file_seek(lfs, file, offset, LFS_SEEK_SET);
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
    rc = lfs_file_tell(lfs, file);
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

    len = (int32_t)lfs_file_size(lfs, file);
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

    size = lfs_file_read(lfs, file, out_data, len);
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

    size = lfs_file_write(lfs, file, data, len);
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

    rc = lfs_remove(&g_littlefs, path);

    return littlefs_to_vfs_error(rc);
}

static int
littlefs_rename(const char *from, const char *to)
{
    int rc;

    if (!from || !to) {
        return FS_EINVAL;
    }

    rc = lfs_rename(&g_littlefs, from, to);

    return littlefs_to_vfs_error(rc);
}

static int
littlefs_mkdir(const char *path)
{
    int rc;

    if (!path) {
        return FS_EINVAL;
    }

    rc = lfs_mkdir(&g_littlefs, path);

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

    rc = lfs_dir_open(&g_littlefs, out_dir, path);
    if (rc < 0) {
        rc = littlefs_to_vfs_error(rc);
        goto out;
    }

    dir->dir = out_dir;
    dir->cur_dirent = NULL;
    dir->fops = &littlefs_ops;
    dir->lfs = &g_littlefs;
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

    rc = lfs_dir_read(lfs, dir, &dirent->info);
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

    rc = lfs_dir_close(lfs, dir);

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
        return 0;
    }

    info = &((struct littlefs_dirent *)fs_dirent)->info;
    return info->type == LFS_TYPE_DIR;
}

int
littlefs_format(void)
{
    int rc;

    rc = lfs_format(&g_littlefs, &g_littlefs_config);
    if (rc) {
        return littlefs_to_vfs_error(rc);
    }

    return FS_EOK;
}

int
littlefs_mount(void)
{
    int rc;

    rc = lfs_mount(&g_littlefs, &g_littlefs_config);
    switch (rc) {
    case LFS_ERR_OK:
        break;
    case LFS_ERR_INVAL:
    case LFS_ERR_CORRUPT:
#if MYNEWT_VAL(LITTLEFS_AUTO_FORMAT)
        rc = lfs_format(&g_littlefs, &g_littlefs_config);
        if (!rc) {
            rc = lfs_mount(&g_littlefs, &g_littlefs_config);
        }
#endif
        break;
    }

    if (rc) {
        return littlefs_to_vfs_error(rc);
    }

    return FS_EOK;
}

int
littlefs_init(void)
{
    struct lfs_config *lfs_cfg = &g_littlefs_config;
    const struct flash_area *fa;
    static struct flash_sector_range fsr;
    int fsr_cnt;
    uint32_t block_count;
    uint32_t block_size;
    int rc;

#ifdef LFS_THREADSAFE
    rc = os_mutex_init(&g_littlefs_mutex);
    if (rc) {
        return FS_EOS;
    }
#endif

    rc = flash_area_open(MYNEWT_VAL(LITTLEFS_FLASH_AREA), &fa);
    if (rc) {
        return FS_EHW;
    }

    lfs_cfg->context = (struct flash_area *)fa;

    fsr_cnt = 1;
    rc = flash_area_to_sector_ranges(FLASH_AREA_STORAGE, &fsr_cnt, &fsr);
    if (rc) {
        return FS_EHW;
    }

    if (lfs_cfg->block_size == 0 && lfs_cfg->block_count == 0) {
        block_size = fsr.fsr_sector_size;
        block_count = fsr.fsr_sector_count;

        /* If sector size is less than required min block size, adjust size and count as needed */
        if (block_size < MIN_BLOCK_SIZE) {
            block_size = max(MIN_BLOCK_SIZE, block_size);
            block_count = fsr.fsr_sector_size * fsr.fsr_sector_count / block_size;
        }

        lfs_cfg->block_size = block_size;
        lfs_cfg->block_count = block_count;
    } else {
        assert(lfs_cfg->block_size > MIN_BLOCK_SIZE);
        assert(lfs_cfg->block_count > 0);
    }

    fs_register(&littlefs_ops);

    return FS_EOK;
}

void
littlefs_sysinit(void)
{
    int rc;

    SYSINIT_ASSERT_ACTIVE();

    rc = littlefs_init();
    SYSINIT_PANIC_ASSERT(rc == 0);

#if MYNEWT_VAL(LITTLEFS_AUTO_MOUNT)
    rc = littlefs_mount();
    SYSINIT_PANIC_ASSERT(rc == 0);
#endif
}
