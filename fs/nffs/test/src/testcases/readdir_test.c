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

TEST_CASE(nffs_test_readdir)
{
    struct fs_dirent *dirent;
    struct fs_dir *dir;
    int rc;

    /*** Setup. */
    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT_FATAL(rc == 0);

    rc = fs_mkdir("/mydir");
    TEST_ASSERT_FATAL(rc == 0);

    nffs_test_util_create_file("/mydir/b", "bbbb", 4);
    nffs_test_util_create_file("/mydir/a", "aaaa", 4);
    rc = fs_mkdir("/mydir/c");
    TEST_ASSERT_FATAL(rc == 0);

    /* Nonexistent directory. */
    rc = fs_opendir("/asdf", &dir);
    TEST_ASSERT(rc == FS_ENOENT);

    /* Fail to opendir a file. */
    rc = fs_opendir("/mydir/a", &dir);
    TEST_ASSERT(rc == FS_EINVAL);

    /* Real directory (with trailing slash). */
    rc = fs_opendir("/mydir/", &dir);
    TEST_ASSERT_FATAL(rc == 0);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "a");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 0);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "b");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 0);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "c");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 1);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_closedir(dir);
    TEST_ASSERT(rc == 0);

    /* Root directory. */
    rc = fs_opendir("/", &dir);
    TEST_ASSERT(rc == 0);
    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_ent_name(dirent, "lost+found");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 1);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_ent_name(dirent, "mydir");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 1);

    rc = fs_closedir(dir);
    TEST_ASSERT(rc == 0);

    /* Delete entries while iterating. */
    rc = fs_opendir("/mydir", &dir);
    TEST_ASSERT_FATAL(rc == 0);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_ent_name(dirent, "a");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 0);

    rc = fs_unlink("/mydir/b");
    TEST_ASSERT(rc == 0);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == 0);

    rc = fs_unlink("/mydir/c");
    TEST_ASSERT(rc == 0);

    rc = fs_unlink("/mydir");
    TEST_ASSERT(rc == 0);

    nffs_test_util_assert_ent_name(dirent, "c");
    TEST_ASSERT(fs_dirent_is_dir(dirent) == 1);

    rc = fs_readdir(dir, &dirent);
    TEST_ASSERT(rc == FS_ENOENT);

    rc = fs_closedir(dir);
    TEST_ASSERT(rc == 0);

    /* Ensure directory is gone. */
    rc = fs_opendir("/mydir", &dir);
    TEST_ASSERT(rc == FS_ENOENT);
}
