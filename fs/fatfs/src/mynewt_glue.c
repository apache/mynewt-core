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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <os/mynewt.h>
#include <modlog/modlog.h>
#include <disk/disk.h>

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
static int fatfs_flush(struct fs_file *fs_file);
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
static int fatfs_mount(const file_system_t *fs);
static int fatfs_umount(const file_system_t *fs);

struct fatfs_file {
    struct fs_ops *fops;
    FIL fil;
};

struct fatfs_dirent {
    struct fs_ops *fops;
    FILINFO filinfo;
};

struct fatfs_dir {
    struct fatfs_dirent dirent;
    FATFS_DIR dir;
};

static struct fs_ops fatfs_ops = {
    .f_open = fatfs_open,
    .f_close = fatfs_close,
    .f_read = fatfs_read,
    .f_write = fatfs_write,
    .f_flush = fatfs_flush,

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

    .f_mount = fatfs_mount,
    .f_umount = fatfs_umount,
};

typedef struct fat_disk {
    file_system_t fs;
    char vol[4];
    FATFS fatfs;
    disk_t *disk;
} fat_disk_t;

static fat_disk_t *fat_vol[_VOLUMES];

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

static int
fatfs_open(const char *path, uint8_t access_flags, struct fs_file **out_fs_file)
{
    FRESULT res;
    BYTE mode;
    struct fatfs_file *file = NULL;
    int rc;

    FATFS_LOG_DEBUG("Open file %s", path);

    file = os_malloc(sizeof(struct fatfs_file));
    if (!file) {
        rc = FS_ENOMEM;
        goto out;
    }

    mode = FA_OPEN_EXISTING;
    if (access_flags & FS_ACCESS_READ) {
        mode |= FA_READ;
    }
    if (access_flags & FS_ACCESS_WRITE) {
        mode |= FA_WRITE | FA_OPEN_ALWAYS;
    }
    if (access_flags & FS_ACCESS_APPEND) {
        mode |= FA_OPEN_APPEND;
    }
    if (access_flags & FS_ACCESS_TRUNCATE) {
        mode &= ~FA_OPEN_EXISTING;
        mode |= FA_CREATE_ALWAYS;
    }

    res = f_open(&file->fil, path, mode);
    if (res != FR_OK) {
        rc = fatfs_to_vfs_error(res);
        goto out;
    }

    file->fops = &fatfs_ops;
    *out_fs_file = (struct fs_file *) file;
    rc = FS_EOK;

out:
    if (rc != FS_EOK) {
        FATFS_LOG_ERROR("File %s open failed %d", path, rc);

        if (file) free(file);
    } else {
        FATFS_LOG_DEBUG("File %s opened %p", path, *out_fs_file);
    }
    return rc;
}

static int
fatfs_close(struct fs_file *fs_file)
{
    struct fatfs_file *fat_file = (struct fatfs_file *)fs_file;
    FRESULT res;

    FATFS_LOG_DEBUG("Close file %p", fs_file);

    res = f_close(&fat_file->fil);

    free(fs_file);
    return fatfs_to_vfs_error(res);
}

static int
fatfs_seek(struct fs_file *fs_file, uint32_t offset)
{
    FRESULT res;
    FIL *file = &((struct fatfs_file *)fs_file)->fil;

    FATFS_LOG_DEBUG("File %p seek %u", fs_file, offset);

    res = f_lseek(file, offset);
    return fatfs_to_vfs_error(res);
}

static uint32_t
fatfs_getpos(const struct fs_file *fs_file)
{
    uint32_t offset;
    FIL *file = &((struct fatfs_file *)fs_file)->fil;

    offset = (uint32_t) f_tell(file);
    return offset;
}

static int
fatfs_file_len(const struct fs_file *fs_file, uint32_t *out_len)
{
    FIL *file = &((struct fatfs_file *) fs_file)->fil;

    *out_len = (uint32_t) f_size(file);

    FATFS_LOG_DEBUG("File %p len %u", fs_file, *out_len);

    return FS_EOK;
}

static int
fatfs_read(struct fs_file *fs_file, uint32_t len, void *out_data,
           uint32_t *out_len)
{
    FRESULT res;
    FIL *file = &((struct fatfs_file *)fs_file)->fil;
    UINT uint_len;

    FATFS_LOG_DEBUG("File %p read %u", fs_file, len);

    res = f_read(file, out_data, len, &uint_len);
    *out_len = uint_len;
    return fatfs_to_vfs_error(res);
}

static int
fatfs_write(struct fs_file *fs_file, const void *data, int len)
{
    FRESULT res;
    UINT out_len;
    FIL *file = &((struct fatfs_file *)fs_file)->fil;

    FATFS_LOG_DEBUG("File %p write %u", fs_file, len);

    res = f_write(file, data, len, &out_len);
    if (len != out_len) {
        return FS_EFULL;
    }
    return fatfs_to_vfs_error(res);
}

static int
fatfs_flush(struct fs_file *fs_file)
{
    FRESULT res;
    FIL *file = &((struct fatfs_file *)fs_file)->fil;

    FATFS_LOG_DEBUG("Flush %p", fs_file);

    res = f_sync(file);

    return fatfs_to_vfs_error(res);
}

static int
fatfs_unlink(const char *path)
{
    FRESULT res;

    FATFS_LOG_INFO("Unlink %s", path);

    res = f_unlink(path);

    return fatfs_to_vfs_error(res);
}

static int
fatfs_rename(const char *from, const char *to)
{
    FRESULT res;

    FATFS_LOG_INFO("Rename %s to %s", from, to);

    res = f_rename(from, to);

    return fatfs_to_vfs_error(res);
}

static int
fatfs_mkdir(const char *path)
{
    FRESULT res;

    FATFS_LOG_INFO("Mkdir %s", path);

    res = f_mkdir(path);
    return fatfs_to_vfs_error(res);
}

static int
fatfs_opendir(const char *path, struct fs_dir **out_fs_dir)
{
    FRESULT res;
    struct fatfs_dir *dir = NULL;
    int rc;

    dir = os_malloc(sizeof(struct fatfs_dir));
    if (!dir) {
        rc = FS_ENOMEM;
        goto out;
    }
    dir->dirent.fops = &fatfs_ops;

    res = f_opendir(&dir->dir, path);
    if (res != FR_OK) {
        rc = fatfs_to_vfs_error(res);
        goto out;
    }

    *out_fs_dir = (struct fs_dir *)dir;
    rc = FS_EOK;

    FATFS_LOG_INFO("Open dir %s -> %p", path, *out_fs_dir);

out:
    if (rc != FS_EOK) {
        FATFS_LOG_ERROR("Open dir %s failed %d", path, rc);

        if (dir) {
            os_free(dir);
        }
    }
    return rc;
}

static int
fatfs_readdir(struct fs_dir *fs_dir, struct fs_dirent **out_fs_dirent)
{
    FRESULT res;
    struct fatfs_dir *fat_dir = ((struct fatfs_dir *)fs_dir);
    FATFS_DIR *dir = &fat_dir->dir;

    FATFS_LOG_DEBUG("Read dir %p", fs_dir);

    res = f_readdir(dir, &fat_dir->dirent.filinfo);
    if (res != FR_OK) {
        return fatfs_to_vfs_error(res);
    }

    *out_fs_dirent = (struct fs_dirent *)&fat_dir->dirent;
    if (!fat_dir->dirent.filinfo.fname[0]) {
        return FS_ENOENT;
    }
    return FS_EOK;
}

static int
fatfs_closedir(struct fs_dir *fs_dir)
{
    FRESULT res;
    FATFS_DIR *dir = &((struct fatfs_dir *) fs_dir)->dir;

    FATFS_LOG_INFO("Close dir %p", fs_dir);

    res = f_closedir(dir);

    os_free(fs_dir);
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

static int
fatfs_mount(const file_system_t *fs)
{
    int rc;
    fat_disk_t *fat_disk = (fat_disk_t *)fs;

    /* Pass to FAT driver */
    rc = f_mount(&fat_disk->fatfs, fat_disk->vol, 0);

    return rc;
}

static int
fatfs_umount(const file_system_t *fs)
{
    int rc;
    fat_disk_t *fat_disk = (fat_disk_t *)fs;

    /* Pass to FAT driver */
    rc = f_mount(NULL, fat_disk->vol, 0);

    return rc;
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

DRESULT
disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count)
{
    int rc;
    struct fat_disk *fat_disk = fat_vol[pdrv];

    if (fat_disk != NULL) {
        rc = mn_disk_read(fat_disk->disk, sector, buff, count);
        if (rc != 0) {
            rc = FR_NOT_READY;
        }
    } else {
        rc = FR_NOT_READY;
    }

    return rc;
}

DRESULT
disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count)
{
    int rc;
    struct fat_disk *fat_disk = fat_vol[pdrv];

    if (fat_disk != NULL) {
        rc = mn_disk_write(fat_disk->disk, sector, buff, count);
        if (rc != 0) {
            rc = FR_NOT_READY;
        }
    } else {
        rc = FR_NOT_READY;
    }

    return rc;
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

int
ff_cre_syncobj(BYTE vol, _SYNC_t *sobj)
{
    struct os_mutex *mutex = os_malloc(sizeof(*mutex));

    if (mutex) {
        os_mutex_init(mutex);
        *sobj = mutex;
    }

    return mutex != NULL;
}

int
ff_req_grant(_SYNC_t sobj)
{
    struct os_mutex *mutex = (struct os_mutex *)sobj;
    int rc;

    rc = os_mutex_pend(mutex, _FS_TIMEOUT);

    return rc == OS_OK;
}

void
ff_rel_grant(_SYNC_t sobj)
{
    struct os_mutex *mutex = (struct os_mutex *)sobj;

    os_mutex_release(mutex);
}

int
ff_del_syncobj(_SYNC_t sobj)
{
    struct os_mutex *mutex = (struct os_mutex *)sobj;

    assert(mutex->mu_owner == NULL);
    os_free(mutex);

    return FR_OK;
}

void *
ff_memalloc(UINT msize)
{
    return os_malloc(msize);
}

void
ff_memfree(void *mblock)
{
    os_free(mblock);
}

WCHAR
ff_convert(WCHAR chr, UINT dir)
{
    return chr;
}

WCHAR
ff_wtoupper(WCHAR chr)
{
    return toupper(chr);
}

#if MYNEWT_VAL_FATFS_AUTO_MOUNT_DISKS
int fatfs_disk_added(struct disk_listener *listener, struct disk *disk)
{
    fat_disk_t *fat_disk;
    int i;
    int rc;

    for (i = 0; i < _VOLUMES; ++i) {
        if (fat_vol[i] == NULL) {
            break;
        }
    }
    if (i >= _VOLUMES) {
        fat_disk = NULL;
        FATFS_LOG_ERROR("Can mount additional volumes");
        goto end;
    }
    fat_disk = os_malloc(sizeof(*fat_disk));

    if (fat_disk == NULL) {
        FATFS_LOG_ERROR("No enough heap for FAT file system");
        goto end;
    }

    if (mn_disk_read(disk, 0, fat_disk->fatfs.win, 1) == 0) {
        disk_info_t di;
        uint8_t fs_type = fat_disk->fatfs.win[450];
        mn_disk_info_get(disk, &di);

        /* Check if first partition is FAT */
        if (fs_type == 0x0C || fs_type == 0x0B || (_FS_EXFAT && fs_type == 0x07)) {
            fat_disk->disk = disk;
            fat_disk->fs.ops = &fatfs_ops;
            fat_disk->fs.name = "fatfs";
            fat_disk->vol[0] = '0' + i;
            fat_disk->vol[1] = ':';
            fat_disk->vol[2] = '\0';

            /* Associate in mynewt */
            rc = fs_mount(&fat_disk->fs, fat_disk->vol);
            if (rc != 0) {
                FATFS_LOG_ERROR("Can't mount FAT file system");
            } else {
                fat_vol[i] = fat_disk;
                /* Disk successfully mounted, prevent from being freed */
                FATFS_LOG_INFO("Mounted FAT partition from %s at %s", di.name, fat_disk->vol);
                fat_disk = NULL;
            }
        } else {
            FATFS_LOG_INFO("Disk %s not with FAT file system", di.name);
        }
    }
end:
    if (fat_disk) {
        os_free(fat_disk);
    }
    return 0;
}

int fatfs_disk_removed(struct disk_listener *listener, struct disk *disk)
{
    int i;
    int rc = FS_EINVAL;
    struct fat_disk *fat_disk;

    for (i = 0; i < _VOLUMES; ++i) {
        fat_disk = fat_vol[i];
        if (fat_disk->disk == disk) {
            fs_unmount_file_system(&fat_disk->fs);
            fat_vol[i] = NULL;
            os_free(fat_disk);
            rc = 0;
            break;
        }
    }
    return rc;
}

const disk_listener_ops_t fatfs_disk_listener_ops = {
    .disk_added = fatfs_disk_added,
    .disk_removed = fatfs_disk_removed,
};

disk_listener_t fatfs_disk_listener = {
    .ops = &fatfs_disk_listener_ops,
};

DISK_LISTENER(fatfs_disk_listener_ptr, fatfs_disk_listener)

#endif
