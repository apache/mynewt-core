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
#ifndef H_NFFS_TEST_UTILS_
#define H_NFFS_TEST_UTILS_

#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <errno.h>
#include "hal/hal_flash.h"
#include "testutil/testutil.h"
#include "fs/fs.h"
#include "nffs/nffs.h"
#include "nffs_test.h"
#include "nffs_test_priv.h"
#include "nffs_priv.h"

#ifdef __cplusplus
#extern "C" {
#endif

extern struct nffs_hash_entry *nffs_test_touched_entries;
int nffs_test_num_touched_entries;

extern int flash_native_memset(uint32_t offset, uint8_t c, uint32_t len);

void nffs_test_util_assert_ent_name(struct fs_dirent *dirent,
                                    const char *expected_name);
void nffs_test_util_assert_file_len(struct fs_file *file, uint32_t expected);
void nffs_test_util_assert_cache_is_sane(const char *filename);
void nffs_test_util_assert_contents(const char *filename,
                                    const char *contents, int contents_len);
int nffs_test_util_block_count(const char *filename);
void nffs_test_util_assert_block_count(const char *filename,
                                       int expected_count);
void nffs_test_util_assert_cache_range(const char *filename,
                                       uint32_t expected_cache_start,
                                       uint32_t expected_cache_end);
void nffs_test_util_create_file_blocks(const char *filename,
                                   const struct nffs_test_block_desc *blocks,
                                   int num_blocks);
void nffs_test_util_create_file(const char *filename, const char *contents,
                                int contents_len);
void nffs_test_util_append_file(const char *filename, const char *contents,
                                int contents_len);
void nffs_test_copy_area(const struct nffs_area_desc *from,
                         const struct nffs_area_desc *to);
void nffs_test_util_create_subtree(const char *parent_path,
                                   const struct nffs_test_file_desc *elem);
void nffs_test_util_create_tree(const struct nffs_test_file_desc *root_dir);

/*
 * Recursively descend directory structure
 */
void nffs_test_assert_file(const struct nffs_test_file_desc *file,
                           struct nffs_inode_entry *inode_entry,
                           const char *path);
void nffs_test_assert_branch_touched(struct nffs_inode_entry *inode_entry);
void nffs_test_assert_child_inode_present(struct nffs_inode_entry *child);
void nffs_test_assert_block_present(struct nffs_hash_entry *block_entry);
/*
 * Recursively verify that the children of each directory are sorted
 * on the directory children linked list by filename length
 */
void nffs_test_assert_children_sorted(struct nffs_inode_entry *inode_entry);
void nffs_test_assert_system_once(const struct nffs_test_file_desc *root_dir);
void nffs_test_assert_system(const struct nffs_test_file_desc *root_dir,
                             const struct nffs_area_desc *area_descs);
void nffs_test_assert_area_seqs(int seq1, int count1, int seq2, int count2);

#ifdef __cplusplus
}
#endif

#endif /* H_NFFS_TEST_UTILS_ */
