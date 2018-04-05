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

#include <fatfs/ff.h>
#include <fatfs/diskio.h>

#include <fs/fs.h>
#include <fs/fs_if.h>

static int fatfs_open(const char *path, uint8_t access_flags,
  struct fs_file **out_file);
static int fatfs_close(struct fs_file *fs_file);
static int fatfs_read(struct fs_file *fs_file, uint32_t len, void *out_data,
  uint32_t *out_len);
static int fatfs_write(struct fs_file *fs_file, const void *data, int len);
static int fatfs_seek(struct fs_file *fs_file, uint32_t offset);
static uint32_t fatfs_getpos(const struct fs_file *fs_file);
static int fatfs_file_len(const struct fs_file *fs_file, uint32_t *out_len);
static int fatfs_unlink(const char *path);
static int fatfs_rename(const char *from, const char *to);
static int fatfs_mkdir(const char *path);
static int fatfs_opendir(const char *path, struct fs_dir **out_fs_dir);
static int fatfs_readdir(struct fs_dir *dir, struct fs_dirent **out_dirent);
static int fatfs_closedir(struct fs_dir *dir);
static int fatfs_dirent_name(const struct fs_dirent *fs_dirent, size_t max_len,
  char *out_name, uint8_t *out_name_len);
static int fatfs_dirent_is_dir(const struct fs_dirent *fs_dirent);

#define DRIVE_LEN 4

struct fatfs_file {
    struct fs_ops *fops;
    FIL *file;
};

struct fatfs_dir {
    struct fs_ops *fops;
    FATFS_DIR *dir;
};

struct fatfs_dirent {
    struct fs_ops *fops;
    FILINFO filinfo;
};

/* NOTE: to ease memory management of dirent structs, this single static
 * variable holds the latest entry found by readdir. This limits FAT to
 * working on a single thread, single opendir -> closedir cycle.
 */
static struct fatfs_dirent dirent;

static struct fs_ops fatfs_ops = {
    .f_open = fatfs_open,
    .f_close = fatfs_close,
    .f_read = fatfs_read,
    .f_write = fatfs_write,

    .f_seek = fatfs_seek,
    .f_getpos = fatfs_getpos,
    .f_filelen = fatfs_file_len,

    .f_unlink = fatfs_unlink,
    .f_rename = fatfs_rename,
    .f_mkdir = fatfs_mkdir,

    .f_opendir = fatfs_opendir,
    .f_readdir = fatfs_readdir,
    .f_closedir = fatfs_closedir,

    .f_dirent_name = fatfs_dirent_name,
    .f_dirent_is_dir = fatfs_dirent_is_dir,

    .f_name = "fatfs"
};

int fatfs_to_vfs_error(FRESULT res)
{
    int rc = FS_EOS;

    switch (res) {
    case FR_OK:
        rc = FS_EOK;
        break;
    case FR_DISK_ERR:              /* (1) A hard error occurred in the low level disk I/O layer */
        rc = FS_EHW;
        break;
    case FR_INT_ERR:               /* (2) Assertion failed */
        rc = FS_EOS;
        break;
    case FR_NOT_READY:             /* (3) The physical drive cannot work */
        rc = FS_ECORRUPT;
        break;
    case FR_NO_FILE:               /* (4) Could not find the file */
        /* passthrough */
    case FR_NO_PATH:               /* (5) Could not find the path */
        rc = FS_ENOENT;
        break;
    case FR_INVALID_NAME:          /* (6) The path name format is invalid */
        rc = FS_EINVAL;
        break;
    case FR_DENIED:                /* (7) Access denied due to prohibited access or directory full */
        rc = FS_EACCESS;
        break;
    case FR_EXIST:                 /* (8) Access denied due to prohibited access */
        rc = FS_EEXIST;
        break;
    case FR_INVALID_OBJECT:        /* (9) The file/directory object is invalid */
        rc = FS_EINVAL;
        break;
    case FR_WRITE_PROTECTED:       /* (10) The physical drive is write protected */
        /* TODO: assign correct error */
        break;
    case FR_INVALID_DRIVE:         /* (11) The logical drive number is invalid */
        rc = FS_EHW;
        break;
    case FR_NOT_ENABLED:           /* (12) The volume has no work area */
        rc = FS_EUNEXP;
        break;
    case FR_NO_FILESYSTEM:         /* (13) There is no valid FAT volume */
        rc = FS_EUNINIT;
        break;
    case FR_MKFS_ABORTED:          /* (14) The f_mkfs() aborted due to any problem */
        /* TODO: assign correct error */
        break;
    case FR_TIMEOUT:               /* (15) Could not get a grant to access the volume within defined period */
        /* TODO: assign correct error */
        break;
    case FR_LOCKED:                /* (16) The operation is rejected according to the file sharing policy */
        /* TODO: assign correct error */
        break;
    case FR_NOT_ENOUGH_CORE:       /* (17) LFN working buffer could not be allocated */
        rc = FS_ENOMEM;
        break;
    case FR_TOO_MANY_OPEN_FILES:   /* (18) Number of open files > _FS_LOCK */
        /* TODO: assign correct error */
        break;
    case FR_INVALID_PARAMETER:     /* (19) Given parameter is invalid */
        rc = FS_EINVAL;
        break;
    }

    return rc;
}

struct mounted_disk {
    char *disk_name;
    int disk_number;
    struct disk_ops *dops;

    SLIST_ENTRY(mounted_disk) sc_next;
};

static SLIST_HEAD(, mounted_disk) mounted_disks = SLIST_HEAD_INITIALIZER();

static int
drivenumber_from_disk(char *disk_name)
{
    struct mounted_disk *sc;
    struct mounted_disk *new_disk;
    int disk_number;
    FATFS *fs;
    char path[DRIVE_LEN];

    disk_number = 0;
    if (disk_name) {
        SLIST_FOREACH(sc, &mounted_disks, sc_next) {
            if (strcmp(sc->disk_name, disk_name) == 0) {
                return sc->disk_number;
            }
            disk_number++;
        }
    }

    /* XXX: check for errors? */
    fs = malloc(sizeof(FATFS));
    sprintf(path, "%d:", disk_number);
    f_mount(fs, path, 1);

    /* FIXME */
    new_disk = malloc(sizeof(struct mounted_disk));
    new_disk->disk_name = strdup(disk_name);
    new_disk->disk_number = disk_number;
    new_disk->dops = disk_ops_for(disk_name);
    SLIST_INSERT_HEAD(&mounted_disks, new_disk, sc_next);

    return disk_number;
}

static int
fatfs_open(const char *path, uint8_t access_flags, struct fs_file **out_fs_file)
{
    FRESULT res;
    FIL *out_file = NULL;
    BYTE mode;
    struct fatfs_file *file = NULL;
    char *disk;
    int number;
    char *filepath;
    char drivepath[255 + DRIVE_LEN];  /* FIXME */
    int rc;

    file = malloc(sizeof(struct fatfs_file));
    if (!file) {
        rc = FS_ENOMEM;
        goto out;
    }

    out_file = malloc(sizeof(FIL));
    if (!out_file) {
        rc = FS_ENOMEM;
        goto out;
    }

    mode = FA_OPEN_EXISTING;
    if (access_flags & FS_ACCESS_READ) {
        mode |= FA_READ;
    }
    if (access_flags & FS_ACCESS_WRITE) {
        mode |= FA_WRITE;
    }
    if (access_flags & FS_ACCESS_APPEND) {
        mode |= FA_OPEN_APPEND;
    }
    if (access_flags & FS_ACCESS_TRUNCATE) {
        mode &= ~FA_OPEN_EXISTING;
        mode |= FA_CREATE_ALWAYS;
    }

    disk = disk_name_from_path(path);
    number = drivenumber_from_disk(disk);
    filepath = disk_filepath_from_path(path);
    sprintf(drivepath, "%d:%s", number, filepath);
    free(filepath);

    res = f_open(out_file, drivepath, mode);
    if (res != FR_OK) {
        rc = fatfs_to_vfs_error(res);
        goto out;
    }

    file->file = out_file;
    file->fops = &fatfs_ops;
    *out_fs_file = (struct fs_file *) file;
    rc = FS_EOK;

out:
    if (rc != FS_EOK) {
        if (file) free(file);
        if (out_file) free(out_file);
    }
    return rc;
}

static int
fatfs_close(struct fs_file *fs_file)
{
    FRESULT res;
    FIL *file = ((struct fatfs_file *) fs_file)->file;

    if (file == NULL) {
        return FS_EOK;
    }

    res = f_close(file);
    free(file);
    return fatfs_to_vfs_error(res);
}

static int
fatfs_seek(struct fs_file *fs_file, uint32_t offset)
{
    FRESULT res;
    FIL *file = ((struct fatfs_file *) fs_file)->file;

    res = f_lseek(file, offset);
    return fatfs_to_vfs_error(res);
}

static uint32_t
fatfs_getpos(const struct fs_file *fs_file)
{
    uint32_t offset;
    FIL *file = ((struct fatfs_file *) fs_file)->file;

    offset = (uint32_t) f_tell(file);
    return offset;
}

static int
fatfs_file_len(const struct fs_file *fs_file, uint32_t *out_len)
{
    uint32_t offset;
    FIL *file = ((struct fatfs_file *) fs_file)->file;

    offset = (uint32_t) f_size(file);
    return offset;
}

static int
fatfs_read(struct fs_file *fs_file, uint32_t len, void *out_data,
           uint32_t *out_len)
{
    FRESULT res;
    FIL *file = ((struct fatfs_file *) fs_file)->file;
    UINT uint_len;

    res = f_read(file, out_data, len, &uint_len);
    *out_len = uint_len;
    return fatfs_to_vfs_error(res);
}

static int
fatfs_write(struct fs_file *fs_file, const void *data, int len)
{
    FRESULT res;
    UINT out_len;
    FIL *file = ((struct fatfs_file *) fs_file)->file;

    res = f_write(file, data, len, &out_len);
    if (len != out_len) {
        return FS_EFULL;
    }
    return fatfs_to_vfs_error(res);
}

static int
fatfs_unlink(const char *path)
{
    FRESULT res;

    //FIXME: handle disk
    res = f_unlink(path);
    return fatfs_to_vfs_error(res);
}

static int
fatfs_rename(const char *from, const char *to)
{
    FRESULT res;

    //FIXME: handle disk
    res = f_rename(from, to);
    return fatfs_to_vfs_error(res);
}

static int
fatfs_mkdir(const char *path)
{
    FRESULT res;

    res = f_mkdir(path);
    return fatfs_to_vfs_error(res);
}

static int
fatfs_opendir(const char *path, struct fs_dir **out_fs_dir)
{
    FRESULT res;
    FATFS_DIR *out_dir = NULL;
    struct fatfs_dir *dir = NULL;
    char *disk;
    int number;
    char *filepath;
    char drivepath[255 + DRIVE_LEN];  /* FIXME */
    int rc;

    dir = malloc(sizeof(struct fatfs_dir));
    if (!dir) {
        rc = FS_ENOMEM;
        goto out;
    }

    out_dir = malloc(sizeof(FATFS_DIR));
    if (!out_dir) {
        rc = FS_ENOMEM;
        goto out;
    }

    disk = disk_name_from_path(path);
    number = drivenumber_from_disk(disk);
    filepath = disk_filepath_from_path(path);
    sprintf(drivepath, "%d:%s", number, filepath);
    free(filepath);

    res = f_opendir(out_dir, drivepath);
    if (res != FR_OK) {
        rc = fatfs_to_vfs_error(res);
        goto out;
    }

    dir->dir = out_dir;
    dir->fops = &fatfs_ops;
    *out_fs_dir = (struct fs_dir *)dir;
    rc = FS_EOK;

out:
    if (rc != FS_EOK) {
        if (dir) free(dir);
        if (out_dir) free(out_dir);
    }
    return FS_EOK;
}

static int
fatfs_readdir(struct fs_dir *fs_dir, struct fs_dirent **out_fs_dirent)
{
    FRESULT res;
    FATFS_DIR *dir = ((struct fatfs_dir *) fs_dir)->dir;

    dirent.fops = &fatfs_ops;
    res = f_readdir(dir, &dirent.filinfo);
    if (res != FR_OK) {
        return fatfs_to_vfs_error(res);
    }

    *out_fs_dirent = (struct fs_dirent *) &dirent;
    if (!dirent.filinfo.fname[0]) {
        return FS_ENOENT;
    }
    return FS_EOK;
}

static int
fatfs_closedir(struct fs_dir *fs_dir)
{
    FRESULT res;
    FATFS_DIR *dir = ((struct fatfs_dir *) fs_dir)->dir;

    res = f_closedir(dir);
    free(dir);
    return fatfs_to_vfs_error(res);
}

static int
fatfs_dirent_name(const struct fs_dirent *fs_dirent, size_t max_len,
                  char *out_name, uint8_t *out_name_len)
{

    FILINFO *filinfo;
    size_t out_len;

    assert(fs_dirent != NULL);
    filinfo = &(((struct fatfs_dirent *)fs_dirent)->filinfo);
    out_len = max_len < sizeof(filinfo->fname) ? max_len : sizeof(filinfo->fname);
    memcpy(out_name, filinfo->fname, out_len);
    *out_name_len = out_len;
    return FS_EOK;
}

static int
fatfs_dirent_is_dir(const struct fs_dirent *fs_dirent)
{
    FILINFO *filinfo;
    assert(fs_dirent != NULL);
    filinfo = &(((struct fatfs_dirent *)fs_dirent)->filinfo);
    return filinfo->fattrib & AM_DIR;
}

DSTATUS
disk_initialize(BYTE pdrv)
{
    /* Don't need to do anything while using hal_flash */
    return RES_OK;
}

DSTATUS
disk_status(BYTE pdrv)
{
    /* Always OK on native emulated flash */
    return RES_OK;
}

static struct disk_ops *dops_from_handle(BYTE pdrv)
{
    struct mounted_disk *sc;

    SLIST_FOREACH(sc, &mounted_disks, sc_next) {
        if (sc->disk_number == pdrv) {
            return sc->dops;
        }
    }

    return NULL;
}

DRESULT
disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
    int rc;
    uint32_t address;
    uint32_t num_bytes;
    struct disk_ops *dops;

    /* NOTE: safe to assume sector size as 512 for now, see ffconf.h */
    address = (uint32_t) sector * 512;
    num_bytes = (uint32_t) count * 512;

    dops = dops_from_handle(pdrv);
    if (dops == NULL) {
        return STA_NOINIT;
    }

    rc = dops->read(pdrv, address, (void *) buff, num_bytes);
    if (rc < 0) {
        return STA_NOINIT;
    }

    return RES_OK;
}

DRESULT
disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
    int rc;
    uint32_t address;
    uint32_t num_bytes;
    struct disk_ops *dops;

    /* NOTE: safe to assume sector size as 512 for now, see ffconf.h */
    address = (uint32_t) sector * 512;
    num_bytes = (uint32_t) count * 512;

    dops = dops_from_handle(pdrv);
    if (dops == NULL) {
        return STA_NOINIT;
    }

    rc = dops->write(pdrv, address, (const void *) buff, num_bytes);
    if (rc < 0) {
        return STA_NOINIT;
    }

    return RES_OK;
}

DRESULT
disk_ioctl(BYTE pdrv, BYTE cmd, void* buff)
{
    return RES_OK;
}

/* FIXME: _FS_NORTC=1 because there is not hal_rtc interface */
DWORD
get_fattime(void)
{
    return 0;
}

void
fatfs_pkg_init(void)
{
    /* Ensure this function only gets called by sysinit. */
    SYSINIT_ASSERT_ACTIVE();

    fs_register(&fatfs_ops);
}
