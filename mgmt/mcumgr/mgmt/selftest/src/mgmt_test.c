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
#include "mgmt/mgmt.h"

static int
mgmt_test_handler(struct mgmt_ctxt *ctxt)
{
    return 0;
}

static const struct mgmt_handler mgmt_test_handlers[] = {
    [0] = { mgmt_test_handler, NULL },
};

#define MGMT_TEST_GROUP(id) {                                           \
    .mg_handlers = mgmt_test_handlers,                                  \
    .mg_handlers_count = 1,                                             \
    .mg_group_id = (id),                                                \
}

static struct mgmt_group group_a = MGMT_TEST_GROUP(MGMT_GROUP_ID_PERUSER + 0);
static struct mgmt_group group_b = MGMT_TEST_GROUP(MGMT_GROUP_ID_PERUSER + 1);
static struct mgmt_group group_c = MGMT_TEST_GROUP(MGMT_GROUP_ID_PERUSER + 2);
static struct mgmt_group group_d = MGMT_TEST_GROUP(MGMT_GROUP_ID_PERUSER + 3);

static bool
mgmt_test_registered(const struct mgmt_group *group)
{
    return mgmt_find_handler(group->mg_group_id, 0) != NULL;
}

TEST_CASE_SELF(mgmt_test_case_register_unregister)
{
    mgmt_register_group(&group_a);
    mgmt_register_group(&group_b);
    mgmt_register_group(&group_c);
    TEST_ASSERT(mgmt_test_registered(&group_a));
    TEST_ASSERT(mgmt_test_registered(&group_b));
    TEST_ASSERT(mgmt_test_registered(&group_c));

    /* Remove last group and add new one at the end of list. */
    mgmt_unregister_group(&group_c);
    TEST_ASSERT(!mgmt_test_registered(&group_c));
    mgmt_register_group(&group_d);
    TEST_ASSERT(mgmt_test_registered(&group_a));
    TEST_ASSERT(mgmt_test_registered(&group_b));
    TEST_ASSERT(mgmt_test_registered(&group_d));

    /* Remove first group. */
    mgmt_unregister_group(&group_a);
    TEST_ASSERT(!mgmt_test_registered(&group_a));
    TEST_ASSERT(mgmt_test_registered(&group_b));
    TEST_ASSERT(mgmt_test_registered(&group_d));

    /* Remove all groups, list must be usable afterwards. */
    mgmt_unregister_group(&group_d);
    mgmt_unregister_group(&group_b);
    TEST_ASSERT(!mgmt_test_registered(&group_b));
    TEST_ASSERT(!mgmt_test_registered(&group_d));
    mgmt_register_group(&group_c);
    TEST_ASSERT(mgmt_test_registered(&group_c));

    /* Unregistering group that is not registered is no-op. */
    mgmt_unregister_group(&group_a);
    TEST_ASSERT(mgmt_test_registered(&group_c));
}

TEST_SUITE(mgmt_test_suite)
{
    mgmt_test_case_register_unregister();
}

int
main(int argc, char **argv)
{
    mgmt_test_suite();
    return tu_any_failed;
}
