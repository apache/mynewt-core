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

#include "nffs_test_utils.h"

TEST_CASE(nffs_test_unlink)
{
    struct fs_file *file0;
    struct fs_file *file2;
    uint8_t buf[64];
    struct nffs_file *nfs_file;
    uint32_t bytes_read;
    struct fs_file *file1;
    int initial_num_blocks;
    int initial_num_inodes;
    int rc;

    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT_FATAL(rc == 0);

    initial_num_blocks = nffs_block_entry_pool.mp_num_free;
    initial_num_inodes = nffs_inode_entry_pool.mp_num_free;

    nffs_test_util_create_file("/file0.txt", "0", 1);

    rc = fs_open("/file0.txt", FS_ACCESS_READ | FS_ACCESS_WRITE, &file0);
    TEST_ASSERT(rc == 0);
    nfs_file = (struct nffs_file *)file0;
    TEST_ASSERT(nfs_file->nf_inode_entry->nie_refcnt == 2);

    rc = fs_unlink("/file0.txt");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(nfs_file->nf_inode_entry->nie_refcnt == 1);

    rc = fs_open("/file0.txt", FS_ACCESS_READ, &file2);
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_write(file0, "00", 2);
    TEST_ASSERT(rc == 0);

    rc = fs_seek(file0, 0);
    TEST_ASSERT(rc == 0);

    rc = fs_read(file0, sizeof buf, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 2);
    TEST_ASSERT(memcmp(buf, "00", 2) == 0);

    rc = fs_close(file0);
    TEST_ASSERT(rc == 0);


    rc = fs_open("/file0.txt", FS_ACCESS_READ, &file0);
    TEST_ASSERT(rc == FS_ENOENT);

    /* Ensure the file was fully removed from RAM. */
    TEST_ASSERT(nffs_inode_entry_pool.mp_num_free == initial_num_inodes);
    TEST_ASSERT(nffs_block_entry_pool.mp_num_free == initial_num_blocks);

    /*** Nested unlink. */
    rc = fs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);
    nffs_test_util_create_file("/mydir/file1.txt", "1", 2);

    rc = fs_open("/mydir/file1.txt", FS_ACCESS_READ | FS_ACCESS_WRITE, &file1);
    TEST_ASSERT(rc == 0);
    nfs_file = (struct nffs_file *)file1;
    TEST_ASSERT(nfs_file->nf_inode_entry->nie_refcnt == 2);

    rc = fs_unlink("/mydir");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(nfs_file->nf_inode_entry->nie_refcnt == 1);

    rc = fs_open("/mydir/file1.txt", FS_ACCESS_READ, &file2);
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_write(file1, "11", 2);
    TEST_ASSERT(rc == 0);

    rc = fs_seek(file1, 0);
    TEST_ASSERT(rc == 0);

    rc = fs_read(file1, sizeof buf, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 2);
    TEST_ASSERT(memcmp(buf, "11", 2) == 0);

    rc = fs_close(file1);
    TEST_ASSERT(rc == 0);

    rc = fs_open("/mydir/file1.txt", FS_ACCESS_READ, &file1);
    TEST_ASSERT(rc == FS_ENOENT);

    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);

    /* Ensure the files and directories were fully removed from RAM. */
    TEST_ASSERT(nffs_inode_entry_pool.mp_num_free == initial_num_inodes);
    TEST_ASSERT(nffs_block_entry_pool.mp_num_free == initial_num_blocks);
}
