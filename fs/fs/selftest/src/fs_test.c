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

#include "os/mynewt.h"
#include "testutil/testutil.h"
#include "fs/fs.h"
#include "fs/fs_if.h"
#include "mgmt/mgmt.h"
#include "fs_mgmt/fs_mgmt.h"

static int test_fs_mount_rc;

static int
test_fs_mount(const file_system_t *fs)
{
    return test_fs_mount_rc;
}

static const struct fs_ops test_fs_ops = {
    .f_mount = test_fs_mount,
};

static const file_system_t test_fs0 = {
    .ops = &test_fs_ops,
    .name = "test0",
};

static const file_system_t test_fs1 = {
    .ops = &test_fs_ops,
    .name = "test1",
};

static bool
test_fs_mgmt_registered(void)
{
    const struct mgmt_handler *handler;

    handler = mgmt_find_handler(MGMT_GROUP_ID_FS, FS_MGMT_ID_FILE);

    return handler != NULL && handler->mh_read != NULL &&
           handler->mh_write != NULL;
}

TEST_CASE_SELF(fs_test_case_mgmt_registration)
{
    int rc;

    /* fs_mgmt group is registered only after file system is mounted. */
    TEST_ASSERT(!test_fs_mgmt_registered());

    /* Failed mount does not register fs_mgmt group. */
    test_fs_mount_rc = FS_EHW;
    rc = fs_mount(&test_fs0, "test0:");
    TEST_ASSERT(rc == FS_EHW);
    TEST_ASSERT(!test_fs_mgmt_registered());

    /* Successful mount registers fs_mgmt group. */
    test_fs_mount_rc = 0;
    rc = fs_mount(&test_fs0, "test0:");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_fs_mgmt_registered());

    /* Group stays registered when another file system is mounted. */
    rc = fs_mount(&test_fs1, "test1:");
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(test_fs_mgmt_registered());
}

TEST_SUITE(fs_test_suite)
{
    /* Single test case only as mount points are not cleared by sysinit. */
    fs_test_case_mgmt_registration();
}

int
main(int argc, char **argv)
{
    fs_test_suite();
    return tu_any_failed;
}
