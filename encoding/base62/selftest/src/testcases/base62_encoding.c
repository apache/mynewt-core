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

static char plaint_text_01[] = "1";
static char encoded_text_01[] = "n";

static char plaint_text_02[] = "Quick brown fox jumps over the lazy dog";
static char encoded_text_02[] = "1eorxj7biGe3bv0IyYT85oZ2Tivm8BrQyOhZsW9HjnJUifYBtq0Sl";

TEST_CASE_SELF(base62_encoding)
{
    char encoded_text[100];
    char decoded_text[100];

    unsigned int input_len, output_len;
    int rc;

    input_len = strlen(plaint_text_01);
    output_len = 100;
    rc = base62_encode(plaint_text_01, input_len, encoded_text, &output_len);
    TEST_ASSERT(rc == BASE62_DECODE_OK);
    TEST_ASSERT(strlen(encoded_text_01) == output_len);
    encoded_text[output_len] = 0;
    TEST_ASSERT(strcmp(encoded_text, encoded_text_01) == 0);

    input_len = strlen(plaint_text_01);
    output_len = 100;
    rc = base62_decode(encoded_text_01, input_len, decoded_text, &output_len);
    TEST_ASSERT(rc == BASE62_DECODE_OK);
    TEST_ASSERT(strlen(plaint_text_01) == output_len);
    decoded_text[output_len] = 0;
    TEST_ASSERT(strcmp(plaint_text_01, decoded_text) == 0);

    input_len = strlen(plaint_text_02);
    output_len = 100;
    rc = base62_encode(plaint_text_02, input_len, encoded_text, &output_len);
    TEST_ASSERT(rc == BASE62_DECODE_OK);
    TEST_ASSERT(strlen(encoded_text_02) == output_len);
    encoded_text[output_len] = 0;
    TEST_ASSERT(strcmp(encoded_text, encoded_text_02) == 0);

    input_len = strlen(encoded_text_02);
    output_len = 100;
    rc = base62_decode(encoded_text_02, input_len, decoded_text, &output_len);
    TEST_ASSERT(rc == BASE62_DECODE_OK);
    TEST_ASSERT(strlen(plaint_text_02) == output_len);
    decoded_text[output_len] = 0;
    TEST_ASSERT(strcmp(plaint_text_02, decoded_text) == 0);
}
