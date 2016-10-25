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

TEST_CASE(nffs_test_lost_found)
{
    char buf[32];
    struct nffs_inode_entry *inode_entry;
    uint32_t flash_offset;
    uint32_t area_offset;
    uint8_t area_idx;
    int rc;
    struct nffs_disk_inode ndi;
    uint8_t off;    /* calculated offset for memset */

    /*** Setup. */
    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    rc = fs_mkdir("/mydir");
    TEST_ASSERT(rc == 0);
    rc = fs_mkdir("/mydir/dir1");
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/mydir/file1", "aaaa", 4);
    nffs_test_util_create_file("/mydir/dir1/file2", "bbbb", 4);

    /* Corrupt the mydir inode. */
    rc = nffs_path_find_inode_entry("/mydir", &inode_entry);
    TEST_ASSERT(rc == 0);

    snprintf(buf, sizeof buf, "%lu",
             (unsigned long)inode_entry->nie_hash_entry.nhe_id);

    nffs_flash_loc_expand(inode_entry->nie_hash_entry.nhe_flash_loc,
                         &area_idx, &area_offset);
    flash_offset = nffs_areas[area_idx].na_offset + area_offset;
    /*
     * Overwrite the sequence number - should be detected as CRC corruption
     */
    off = (char*)&ndi.ndi_seq - (char*)&ndi;
    rc = flash_native_memset(flash_offset + off, 0xaa, 1);
    TEST_ASSERT(rc == 0);

    /* Clear cached data and restore from flash (i.e, simulate a reboot). */
    rc = nffs_misc_reset();
    TEST_ASSERT(rc == 0);
    rc = nffs_detect(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    /* All contents should now be in the lost+found dir. */
    struct nffs_test_file_desc *expected_system =
        (struct nffs_test_file_desc[]) { {
            .filename = "",
            .is_dir = 1,
            .children = (struct nffs_test_file_desc[]) { {
                .filename = "lost+found",
                .is_dir = 1,
#if 0
                .children = (struct nffs_test_file_desc[]) { {
                    .filename = buf,
                    .is_dir = 1,
                    .children = (struct nffs_test_file_desc[]) { {
                        .filename = "file1",
                        .contents = "aaaa",
                        .contents_len = 4,
                    }, {
                        .filename = "dir1",
                        .is_dir = 1,
                        .children = (struct nffs_test_file_desc[]) { {
                            .filename = "file2",
                            .contents = "bbbb",
                            .contents_len = 4,
                        }, {
                            .filename = NULL,
                        } },
                    }, {
                        .filename = NULL,
                    } },
                }, {
                    .filename = NULL,
                } },
#endif
            }, {
                .filename = NULL,
            } }
    } };

    nffs_test_assert_system(expected_system, nffs_current_area_descs);
}
