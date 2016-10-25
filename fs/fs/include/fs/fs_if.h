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

    const char *f_name;
};

/*
 * Currently allow only one type of FS, starts at root.
 */
int fs_register(const struct fs_ops *);

#ifdef __cplusplus
}
#endif

#endif
