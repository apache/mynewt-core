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

#include "modlog_test.h"

struct mltcb_foreach_arg {
    struct modlog_desc descs[16];
    int num_descs;
};

static int
mltcb_foreach_fn(const struct modlog_desc *desc, void *arg)
{
    struct mltcb_foreach_arg *lfa;

    lfa = arg;
    TEST_ASSERT_FATAL(lfa->num_descs <
                      sizeof lfa->descs / sizeof lfa->descs[0]);

    lfa->descs[lfa->num_descs++] = *desc;
    return 0;
}

static bool
mltcb_desc_has_contents(const struct modlog_desc *desc, uint8_t module,
                        const struct log *log, uint8_t min_level)
{
    return desc->log ==         log     &&
           desc->module ==      module  &&
           desc->min_level ==   min_level;
}

TEST_CASE(modlog_test_case_basic)
{
    struct mltcb_foreach_arg lfa;
    struct log log1;
    struct log log2;
    struct log log3;
    uint8_t handle1;
    uint8_t handle2;
    uint8_t handle3;
    int rc;
    int i;

    sysinit();

    /* NULL log. */
    rc = modlog_register(1, NULL, LOG_LEVEL_DEBUG, NULL);
    TEST_ASSERT(rc == SYS_EINVAL);

    /* Ensure no mappings initially. */
    for (i = 0; i < 256; i++) {
        rc = modlog_get(i, NULL);
        TEST_ASSERT(rc == SYS_ENOENT);
    }

    /* Insert three entries. */
    rc = modlog_register(1, &log1, LOG_LEVEL_DEBUG, &handle1);
    TEST_ASSERT(rc == 0);

    rc = modlog_register(2, &log2, LOG_LEVEL_DEBUG, &handle2);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(handle2 != handle1);

    rc = modlog_register(3, &log3, LOG_LEVEL_DEBUG, &handle3);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(handle3 != handle1);
    TEST_ASSERT(handle3 != handle2);

    /* Ensure foreach applies to all entries in the expected order. */
    memset(&lfa, 0, sizeof lfa);
    rc = modlog_foreach(mltcb_foreach_fn, &lfa);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(lfa.num_descs == 3);
    TEST_ASSERT(mltcb_desc_has_contents(&lfa.descs[0], 1, &log1, LOG_LEVEL_DEBUG));
    TEST_ASSERT(mltcb_desc_has_contents(&lfa.descs[1], 2, &log2, LOG_LEVEL_DEBUG));
    TEST_ASSERT(mltcb_desc_has_contents(&lfa.descs[2], 3, &log3, LOG_LEVEL_DEBUG));

    /* Delete the first entry. */
    rc = modlog_delete(handle1);
    TEST_ASSERT_FATAL(rc == 0);

    memset(&lfa, 0, sizeof lfa);
    rc = modlog_foreach(mltcb_foreach_fn, &lfa);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(lfa.num_descs == 2);
    TEST_ASSERT(mltcb_desc_has_contents(&lfa.descs[0], 2, &log2, LOG_LEVEL_DEBUG));
    TEST_ASSERT(mltcb_desc_has_contents(&lfa.descs[1], 3, &log3, LOG_LEVEL_DEBUG));

    /* Modify entry 3 to point to log1. */
    rc = modlog_delete(handle3);
    TEST_ASSERT_FATAL(rc == 0);
    rc = modlog_register(3, &log1, LOG_LEVEL_DEBUG, &handle3);
    TEST_ASSERT(rc == 0);

    memset(&lfa, 0, sizeof lfa);
    rc = modlog_foreach(mltcb_foreach_fn, &lfa);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(lfa.num_descs == 2);
    TEST_ASSERT(mltcb_desc_has_contents(&lfa.descs[0], 2, &log2, LOG_LEVEL_DEBUG));
    TEST_ASSERT(mltcb_desc_has_contents(&lfa.descs[1], 3, &log1, LOG_LEVEL_DEBUG));

    /* Delete entry 3. */
    rc = modlog_delete(handle3);
    TEST_ASSERT_FATAL(rc == 0);

    memset(&lfa, 0, sizeof lfa);
    rc = modlog_foreach(mltcb_foreach_fn, &lfa);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(lfa.num_descs == 1);
    TEST_ASSERT(mltcb_desc_has_contents(&lfa.descs[0], 2, &log2, LOG_LEVEL_DEBUG));

    /* Delete last entry. */
    rc = modlog_delete(handle2);
    TEST_ASSERT_FATAL(rc == 0);

    memset(&lfa, 0, sizeof lfa);
    rc = modlog_foreach(mltcb_foreach_fn, &lfa);
    TEST_ASSERT(rc == 0);
    TEST_ASSERT(lfa.num_descs == 0);

    /* Repeatedly add and remove entries; verify no leaks. */
    for (i = 0; i < 100; i++) {
        rc = modlog_register(1, &log1, LOG_LEVEL_DEBUG, &handle1);
        TEST_ASSERT(rc == 0);

        rc = modlog_delete(handle1);
        TEST_ASSERT(rc == 0);
    }
}
