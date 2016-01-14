/**
 * Copyright (c) 2015 Runtime Inc.
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
#include <stdio.h>
#include <string.h>

#include "os/os.h"
#include "testutil/testutil.h"
#include "util/config.h"

uint8_t val8;

static struct conf_entry ce1 = {
    .c_name = "ce1",
    .c_type = CONF_INT8,
    .c_val.single.val = &val8
};

static struct conf_node cn1 = {
    .cn_cnt = 1,
    .cn_array = &ce1
};

static struct conf_entry ce2 = {
    .c_name = "ce2",
    .c_type = CONF_INT8,
    .c_val.single.val = &val8
};

static struct conf_node cn2 = {
    .cn_cnt = 1,
    .cn_array = &ce2
};

static struct conf_entry ce_arr1[2] = {
    [0] = {
        .c_name = "cea1",
        .c_type = CONF_INT8,
        .c_val.single.val = &val8
    },
    [1] = {
        .c_name = "cea2",
        .c_type = CONF_INT8,
        .c_val.single.val = &val8
    }
};

static struct conf_node cn_arr1 = {
    .cn_cnt = 2,
    .cn_array = ce_arr1
};

static struct conf_entry ce_arr2[2] = {
    [0] = {
        .c_name = "ce21",
        .c_type = CONF_INT8,
        .c_val.single.val = &val8
    },
    [1] = {
        .c_name = "cea2",
        .c_type = CONF_INT8,
        .c_val.single.val = &val8
    }
};

static struct conf_node cn_arr2 = {
    .cn_cnt = 2,
    .cn_array = ce_arr2
};

static struct conf_entry_dir ce_dir = {
    .c_name = "foo",
    .c_type = CONF_DIR
};

static struct conf_node cn_dir = {
    .cn_cnt = 1,
    .cn_array = (struct conf_entry *)&ce_dir
};

static struct conf_entry ce_foo_arr1[2] = {
    [0] = {
        .c_name = "foo1",
        .c_type = CONF_INT8,
        .c_val.single.val = &val8
    },
    [1] = {
        .c_name = "foo2",
        .c_type = CONF_INT8,
        .c_val.single.val = &val8
    }
};

static struct conf_node cn_foo_arr1 = {
    .cn_cnt = 2,
    .cn_array = ce_foo_arr1
};

TEST_CASE(config_empty_lookups)
{
    struct conf_entry *ce;
    char *names1[] = { "foo", "bar" };

    ce = conf_lookup(0, NULL);
    TEST_ASSERT(ce == NULL);

    ce = conf_lookup(1, names1);
    TEST_ASSERT(ce == NULL);

    ce = conf_lookup(2, names1);
    TEST_ASSERT(ce == NULL);
}

TEST_CASE(config_test_insert)
{
    int rc;

    /*
     * Add 2 new ones
     */
    rc = conf_register(NULL, &cn1);
    TEST_ASSERT(rc == 0);
    rc = conf_register(NULL, &cn2);
    TEST_ASSERT(rc == 0);

    /*
     * Fail adding same ones again
     */
    rc = conf_register(NULL, &cn1);
    TEST_ASSERT(rc != 0);
    rc = conf_register(NULL, &cn2);
    TEST_ASSERT(rc != 0);

    /*
     * Add with multiple conf_entries
     */
    rc = conf_register(NULL, &cn_arr1);
    TEST_ASSERT(rc == 0);

    /*
     * Cannot add it again
     */
    rc = conf_register(NULL, &cn_arr1);
    TEST_ASSERT(rc != 0);

    /*
     * Should fail right away
     */
    rc = conf_register(NULL, &cn_arr2);
    TEST_ASSERT(rc != 0);

}

TEST_CASE(config_test_lookup)
{
    struct conf_entry *ce;
    char *names1[] = { "foo", "bar" };
    char *names2[] = { "ce1" };
    char *names3[] = { "cea2" };

    ce = conf_lookup(0, NULL);
    TEST_ASSERT(ce == NULL);

    ce = conf_lookup(1, names1);
    TEST_ASSERT(ce == NULL);

    ce = conf_lookup(2, names1);
    TEST_ASSERT(ce == NULL);

    ce = conf_lookup(1, names2);
    TEST_ASSERT(ce != NULL);
    TEST_ASSERT(!strcmp(ce->c_name, names2[0]));

    ce = conf_lookup(1, names3);
    TEST_ASSERT(ce != NULL);
    TEST_ASSERT(!strcmp(ce->c_name, names3[0]));
}

TEST_CASE(config_test_dir)
{
    int rc;
    struct conf_entry *ce;
    char *names1[] = { "foo", "foo1" };
    char *names2[] = { "foo", "foo2" };
    char *names3[] = { "foo", "foo3" };

    /*
     * Add directory node, and node under.
     */
    rc = conf_register(NULL, &cn_dir);
    TEST_ASSERT(rc == 0);
    rc = conf_register(&cn_dir, &cn_foo_arr1);
    TEST_ASSERT(rc == 0);

    ce = conf_lookup(1, names1);
    TEST_ASSERT(ce != NULL);
    TEST_ASSERT(ce->c_type == CONF_DIR);

    ce = conf_lookup(2, names1);
    TEST_ASSERT(ce != NULL);
    TEST_ASSERT(!strcmp(ce->c_name, names1[1]));

    ce = conf_lookup(2, names2);
    TEST_ASSERT(ce != NULL);
    TEST_ASSERT(!strcmp(ce->c_name, names2[1]));

    ce = conf_lookup(2, names3);
    TEST_ASSERT(ce == NULL);
}

TEST_SUITE(config_test_suite)
{
    config_empty_lookups();
    config_test_insert();
    config_test_lookup();
    config_test_dir();
}
