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
#include "test_json_priv.h"

TEST_CASE_SELF(test_json_simple_encode)
{
    struct json_encoder encoder;
    struct json_value value;
    int rc;

    /* reset the state of the internal test */
    buf_index = 0;
    memset(&encoder, 0, sizeof(encoder));

    encoder.je_write = test_write;
    encoder.je_arg= NULL;

    rc = json_encode_object_start(&encoder);
    TEST_ASSERT(rc == 0);

    JSON_VALUE_BOOL(&value, 1);
    rc = json_encode_object_entry(&encoder, "KeyBool", &value);
    TEST_ASSERT(rc == 0);

    JSON_VALUE_INT(&value, -1234);
    rc = json_encode_object_entry(&encoder, "KeyInt", &value);
    TEST_ASSERT(rc == 0);

    JSON_VALUE_UINT(&value, 1353214);
    rc = json_encode_object_entry(&encoder, "KeyUint", &value);
    TEST_ASSERT(rc == 0);

    JSON_VALUE_STRING(&value, "foobar");
    rc = json_encode_object_entry(&encoder, "KeyString", &value);
    TEST_ASSERT(rc == 0);

    /* we'll decode later differently */
    JSON_VALUE_STRINGN(&value, "foobarlongstring", 10);
    rc = json_encode_object_entry(&encoder, "KeyStringN", &value);
    TEST_ASSERT(rc == 0);

    rc = json_encode_array_name(&encoder, "KeyIntArr");
    TEST_ASSERT(rc == 0);

    rc = json_encode_array_start(&encoder);
    TEST_ASSERT(rc == 0);

    JSON_VALUE_INT(&value, 153);
    rc = json_encode_array_value(&encoder, &value);
    TEST_ASSERT(rc == 0);

    JSON_VALUE_INT(&value, 2532);
    rc = json_encode_array_value(&encoder, &value);
    TEST_ASSERT(rc == 0);

    JSON_VALUE_INT(&value, -322);
    rc = json_encode_array_value(&encoder, &value);
    TEST_ASSERT(rc == 0);

    rc = json_encode_array_finish(&encoder);
    TEST_ASSERT(rc == 0);

    rc = json_encode_object_finish(&encoder);
    TEST_ASSERT(rc == 0);

    bigbuf[buf_index] = '\0';

    /* does it match what we expect it to */
    rc = strcmp(bigbuf, output);
    TEST_ASSERT(rc == 0);
}
