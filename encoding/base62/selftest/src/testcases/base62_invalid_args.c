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
#include "testutil/testutil.h"

#include <base62/base62.h>

TEST_CASE_SELF(base62_invalid_args)
{
    char plain_text[] = "Quick brown fox jumps over the lazy dog";
    char encoded_text[100];

    unsigned int input_len, output_len;
    int rc;

    input_len = strlen(plain_text);
    output_len = 100;
    rc = base62_encode(plain_text, input_len, NULL, &output_len);
    TEST_ASSERT(rc == BASE62_INVALID_ARG);

    output_len = 100;
    rc = base62_encode(NULL, input_len, encoded_text, &output_len);
    TEST_ASSERT(rc == BASE62_INVALID_ARG);

    output_len = 100;
    rc = base62_encode(plain_text, input_len, encoded_text, NULL);
    TEST_ASSERT(rc == BASE62_INVALID_ARG);

    output_len = input_len - 1;
    rc = base62_encode(plain_text, input_len, encoded_text, &output_len);
    TEST_ASSERT(rc == BASE62_INVALID_ARG);
}
