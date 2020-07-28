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
#include "testutil/testutil.h"

static const struct test_data {
    const char *format;
    long long number;
    const char *result;
} tests[] = {
/* hex */
    {
        .format = "%hx",
        .number = 0x2a,
        .result = "2a",
    },
    {
        .format = "%hx",
        .number = 0xff,
        .result = "ff",
    },
    {
        .format = "%hx",
        .number = 0xffff,
        .result = "ffff",
    },
    {
        .format = "%hx",
        .number = 0x10000,
        .result = "0",
    },
    {
        .format = "%hx",
        .number = 0x1ffff,
        .result = "ffff",
    },
    {
        .format = "%hhx",
        .number = 0xff,
        .result = "ff",
    },
    {
        .format = "%hhx",
        .number = 0x100,
        .result = "0",
    },
    {
        .format = "%hhx",
        .number = 0x1ff,
        .result = "ff",
    },
    {
        .format = "%hhx",
        .number = 0x1ff,
        .result = "ff",
    },
/* HEX */
    {
        .format = "%hX",
        .number = 0x2a,
        .result = "2A",
    },
    {
        .format = "%hX",
        .number = 0xff,
        .result = "FF",
    },
    {
        .format = "%hX",
        .number = 0xffff,
        .result = "FFFF",
    },
    {
        .format = "%hX",
        .number = 0x10000,
        .result = "0",
    },
    {
        .format = "%hX",
        .number = 0x1ffff,
        .result = "FFFF",
    },
    {
        .format = "%hhX",
        .number = 0xff,
        .result = "FF",
    },
    {
        .format = "%hhX",
        .number = 0x100,
        .result = "0",
    },
    {
        .format = "%hhX",
        .number = 0x1ff,
        .result = "FF",
    },
    {
        .format = "%hhX",
        .number = 0x1ff,
        .result = "FF",
    },
/* unsigned decimal */
    {
        .format = "%hu",
        .number = 42,
        .result = "42",
    },
    {
        .format = "%hu",
        .number = 255,
        .result = "255",
    },
    {
        .format = "%hu",
        .number = 65535,
        .result = "65535",
    },
    {
        .format = "%hu",
        .number = 65536,
        .result = "0",
    },
    {
        .format = "%hu",
        .number = 131071,
        .result = "65535",
    },
    {
        .format = "%hhu",
        .number = 42,
        .result = "42",
    },
    {
        .format = "%hhu",
        .number = 255,
        .result = "255",
    },
    {
        .format = "%hhu",
        .number = 256,
        .result = "0",
    },
    {
        .format = "%hhu",
        .number = 511,
        .result = "255",
    },
/* signed decimal */
    {
        .format = "%hd",
        .number = 42,
        .result = "42",
    },
    {
        .format = "%hd",
        .number = -42,
        .result = "-42",
    },
    {
        .format = "%hd",
        .number = 32767,
        .result = "32767",
    },
    {
        .format = "%hd",
        .number = -32768,
        .result = "-32768",
    },
    {
        .format = "%hd",
        .number = 32767 + 65536,
        .result = "32767",
    },
    {
        .format = "%hd",
        .number = -32768 - 65536,
        .result = "-32768",
    },
    {
        .format = "%hhd",
        .number = 42,
        .result = "42",
    },
    {
        .format = "%hhd",
        .number = -42,
        .result = "-42",
    },
    {
        .format = "%hhd",
        .number = 127,
        .result = "127",
    },
    {
        .format = "%hhd",
        .number = -128,
        .result = "-128",
    },
    {
        .format = "%hhd",
        .number = 127 + 256,
        .result = "127",
    },
    {
        .format = "%hhd",
        .number = -127 - 256,
        .result = "-127",
    },
};

TEST_CASE_SELF(tinyprintf_test)
{
    char result[100];
    int i;

    for (i = 0; i < ARRAY_SIZE(tests); i++) {
        memset(result, 0, sizeof(result));
        snprintf(result, sizeof(result), tests[i].format, tests[i].number);

        TEST_ASSERT(strcmp(result, tests[i].result) == 0);
    }
}
