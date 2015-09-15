/**
 * Copyright (c) 2015 Stack Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef H_FFS_TEST_PRIV_
#define H_FFS_TEST_PRIV_

struct ffs_test_block_desc {
    const char *data;
    int data_len;
};

struct ffs_test_file_desc {
    const char *filename;
    int is_dir;
    const char *contents;
    int contents_len;
    struct ffs_test_file_desc *children;
};

int ffs_test(void);

extern const struct ffs_test_file_desc *ffs_test_system_01;
extern const struct ffs_test_file_desc *ffs_test_system_01_rm_1014_mk10;

#endif

