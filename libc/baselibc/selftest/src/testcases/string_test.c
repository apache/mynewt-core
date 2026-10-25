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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "testutil/testutil.h"

TEST_CASE_SELF(strndup_test)
{
    char src[16] = "abc";
    char *s;

    /* n larger than string length */
    s = strndup(src, 10);
    TEST_ASSERT_FATAL(s != NULL);
    TEST_ASSERT(!strcmp(s, "abc"));
    free(s);

    /* n smaller than string length */
    s = strndup("abcdef", 3);
    TEST_ASSERT_FATAL(s != NULL);
    TEST_ASSERT(!strcmp(s, "abc"));
    free(s);

    s = strndup("abc", 0);
    TEST_ASSERT_FATAL(s != NULL);
    TEST_ASSERT(!strcmp(s, ""));
    free(s);
}

TEST_CASE_SELF(strnstr_test)
{
    const char *h = "abcdef";

    TEST_ASSERT(strnstr(h, "cd", 6) == h + 2);
    TEST_ASSERT(strnstr(h, "cd", 4) == h + 2);
    /* Needle not fully within first len characters */
    TEST_ASSERT(strnstr(h, "cd", 3) == NULL);
    TEST_ASSERT(strnstr(h, "ef", 6) == h + 4);
}

TEST_CASE_SELF(sscanf_set_test)
{
    struct {
        char buf[4];
        char guard[8];
    } s;
    char buf[16];
    int rc;

    /* Range includes both ends */
    rc = sscanf("az-", "%[a-z]", buf);
    TEST_ASSERT(rc == 1);
    TEST_ASSERT(!strcmp(buf, "az"));

    /* Trailing '-' is literal */
    rc = sscanf("a-b", "%[a-]", buf);
    TEST_ASSERT(rc == 1);
    TEST_ASSERT(!strcmp(buf, "a-"));

    /* Width limits number of characters stored */
    memset(&s, 0x5a, sizeof(s));
    rc = sscanf("123456789", "%3[0-9]", s.buf);
    TEST_ASSERT(rc == 1);
    TEST_ASSERT(!strcmp(s.buf, "123"));
    TEST_ASSERT(s.guard[0] == 0x5a);
}
