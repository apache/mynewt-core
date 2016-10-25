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

TEST_CASE(nffs_test_read)
{
    struct fs_file *file;
    uint8_t buf[16];
    uint32_t bytes_read;
    int rc;

    rc = nffs_format(nffs_current_area_descs);
    TEST_ASSERT(rc == 0);

    nffs_test_util_create_file("/myfile.txt", "1234567890", 10);

    rc = fs_open("/myfile.txt", FS_ACCESS_READ, &file);
    TEST_ASSERT(rc == 0);
    nffs_test_util_assert_file_len(file, 10);
    TEST_ASSERT(fs_getpos(file) == 0);

    rc = fs_read(file, 4, buf, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 4);
    TEST_ASSERT(memcmp(buf, "1234", 4) == 0);
    TEST_ASSERT(fs_getpos(file) == 4);

    rc = fs_read(file, sizeof buf - 4, buf + 4, &bytes_read);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(bytes_read == 6);
    TEST_ASSERT(memcmp(buf, "1234567890", 10) == 0);
    TEST_ASSERT(fs_getpos(file) == 10);

    rc = fs_close(file);
    TEST_ASSERT(rc == 0);
}
