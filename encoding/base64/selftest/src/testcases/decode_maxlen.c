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
#include "base64_test_priv.h"

static void
pass(const char *src, int max_len, const char *expected)
{
    char dst[1024];
    int len;

    len = base64_decode_maxlen(src, dst, max_len);
    TEST_ASSERT_FATAL(len == strlen(expected));
    TEST_ASSERT(memcmp(dst, expected, len) == 0);
}

TEST_CASE_SELF(decode_maxlen)
{
    pass("dGhlIGRpZSBpcyBjYXN0", 5, "the d");
    pass("c29tZSB0ZXh0IHdpdGggcGFkZGluZw==", 10, "some text ");
    pass("c29tZSB0ZXh0IHdpdGggcGFkZGluZw==", 1000, "some text with padding");
}
