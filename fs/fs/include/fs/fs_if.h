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

#ifndef __FS_IF_H__
#define __FS_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "os/mynewt.h"

typedef struct file_system {
    const struct fs_ops *ops;
    const char *name;
} file_system_t;

struct mount_point {
    const char *mount_point;
    const file_system_t *fs;
};

/*
 * Common interface filesystem(s) provide.
 */
struct fs_ops {
    int (*f_open)(const char *filename, uint8_t access_flags,
              struct fs_file **out_file);
    int (*f_close)(struct fs_file *file);
    int (*f_read)(struct fs_file *file, uint32_t len, void *out_data,
      uint32_t *out_len);
    int (*f_write)(struct fs_file *file, const void *data, int len);
    int (*f_flush)(struct fs_file *file);

    int (*f_seek)(struct fs_file *file, uint32_t offset);
    uint32_t (*f_getpos)(const struct fs_file *file);
    int (*f_filelen)(const struct fs_file *file, uint32_t *out_len);

    int (*f_unlink)(const char *filename);
    int (*f_rename)(const char *from, const char *to);
    int (*f_mkdir)(const char *path);

    int (*f_opendir)(const char *path, struct fs_dir **out_dir);
    int (*f_readdir)(struct fs_dir *dir, struct fs_dirent **out_dirent);
    int (*f_closedir)(struct fs_dir *dir);

    int (*f_dirent_name)(const struct fs_dirent *dirent, size_t max_len,
      char *out_name, uint8_t *out_name_len);
    int (*f_dirent_is_dir)(const struct fs_dirent *dirent);
    int (*f_mount)(const file_system_t *fs);
    int (*f_umount)(const file_system_t *fs);
};

struct fops_container {
    struct fs_ops *fops;
};

/**
 * Registers a new filesystem interface
 *
 * @param fops filesystem operations table
 *
 * @return 0 on success, non-zero on failure
 */
int fs_register(struct fs_ops *fops);

/**
 * Will look for the number of registered filesystems and will return
 * the fops if there is only one.
 *
 * @return fops if there's only one registered filesystem, NULL otherwise.
 */
const struct fs_ops *fs_ops_try_unique(void);

const struct fs_ops *fs_ops_from_container(struct fops_container *container);

/**
 * Mount file system
 *
 * @param fs - file system to mount
 * @param path - path to mount i.e. 0:, nffs:
 * @return 0 on success
 */
int fs_mount(const file_system_t *fs, const char *path);

/**
 * Unmount file system by mount point
 *
 * @param mount_point - mount point to unmount i.e. 0:, nffs:
 * @return 0 on success
 */
const file_system_t *fs_unmount_mount_point(const char *mount_point);

/**
 * Unmount file system
 *
 * @param fs - mounted file system to unmount
 * @return 0 on success
 */
int fs_unmount_file_system(const file_system_t *fs);

/**
 * Get file system mount point path
 *
 * For given path it will return mount point and file system
 * i.e.:
 * For 0:/DIREC/FIL.TXT function will return "0:" and fs
 * will be filled with pointer to valid file_system_t structure.
 *
 * @param path - file or directory name
 * @param fs - on success file system
 * @return file system mount point string
 */
const char *file_system_path(const char *path, const file_system_t **fs);

/**
 * Return the only file system mounted
 *
 * It there is only one file system mounted it will be returned by this
 * function. If two file system were mounted funciton will return NULL.
 * @return non-NULL if exactly one file system is mounted, otherwise NULL
 */
const file_system_t *get_only_file_system(void);

/**
 * Get file system and file system mount point
 *
 * Usually
 * @param path - file system path
 * @param fs - file system for path
 * @param fs_path - mount point (usually prefix of file path)
 */
void get_file_system_path(const char *path, const file_system_t **fs, const char **fs_path);

#ifdef __cplusplus
}
#endif

#endif
