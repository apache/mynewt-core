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

#include "json/json.h"
#include "test_json_priv.h"

static long long unsigned int uint_val;
static long long int int_val;
static bool bool_val;
static char string1[16];
static char string2[16];
static long long int intarr[8];
static int array_count;

static struct json_attr_t test_attr[7] = {
    [0] = {
        .attribute = "KeyBool",
        .type = t_boolean,
        .addr.boolean = &bool_val,
        .nodefault = true
    },
    [1] = {
        .attribute = "KeyInt",
        .type = t_integer,
        .addr.integer = &int_val,
        .nodefault = true
        },
    [2] = {
        .attribute = "KeyUint",
        .type = t_uinteger,
        .addr.uinteger = &uint_val,
        .nodefault = true
        },
    [3] = {
        .attribute = "KeyString",
        .type = t_string,
        .addr.string = string1,
        .nodefault = true,
        .len = sizeof(string1)
        },
    [4] = {
        .attribute = "KeyStringN",
        .type = t_string,
        .addr.string = string2,
        .nodefault = true,
        .len = sizeof(string2)
    },
    [5] = {
        .attribute = "KeyIntArr",
        .type = t_array,
        .addr.array = {
            .element_type = t_integer,
            .arr.integers.store = intarr,
            .maxlen = sizeof intarr / sizeof intarr[0],
            .count = &array_count,
        },
        .nodefault = true,
        .len = sizeof(intarr)
    },
    [6] = {
        .attribute = NULL
    }
};

int
json_test_decode(const char *json_str)
{
    struct test_jbuf tjb;

    test_buf_init(&tjb, json_str);

    return json_read_object(&tjb.json_buf, test_attr);
}

static const char json_ok1[] =
    "{\"KeyBool\": true,\"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": \"foobar\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_ok2[] =
    "{\"KeyBool\": false,\"KeyInt\": 9223372036854775807,\"KeyUint\": 18446744073709551615,\"KeyString\": \"foobar\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_ok3[] =
    "{\"KeyBool\": false,\"KeyInt\": -9223372036854775808,\"KeyUint\": 1353214,\"KeyString\": \"foobar\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_int_out_of_range1[] =
    "{\"KeyBool\": false,\"KeyInt\": -9223372036854775809,\"KeyUint\": 1353214,\"KeyString\": \"foobar\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_int_out_of_range2[] =
    "{\"KeyBool\": false,\"KeyInt\": 9223372036854775807,\"KeyUint\": 18446744073709551616,\"KeyString\": \"foobar\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_bool_fail[] =
    "{\"KeyBool\": truea,\"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": \"foobar\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_missing_val[] =
    "{\"KeyBool\":,\"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": \"foobar\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_str_to_long1[] =
    "{\"KeyBool\": false, \"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": \"12345678901234567\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_str_to_long2[] =
    "{\"KeyBool\": false, \"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": \"1234567890123456\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_str_no_quotes[] =
    "{\"KeyBool\": false, \"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": 123456789012345,\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_extra_coma0[] =
    "{\"KeyBool\":false, , \"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": 123456789012345,\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_extra_coma1[] =
    "{\"KeyBool\": false, \"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": 123456789012345,\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322],}";
static const char json_extra_coma2[] =
    "{\"KeyBool\": false, \"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": 123456789012345,\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322,]}";
static const char json_missing_quote[] =
    "{\"KeyBool\": false, \"KeyInt\": -1234,\"KeyUint: 1353214,\"KeyString\": 123456789012345,\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322],}";
static const char json_attr_to_long[] =
    "{\"A2345678901234567890123456789012\": false, \"KeyInt\": -1234,\"KeyUint: 1353214,\"KeyString\": 123456789012345,\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_bad_attr[] =
    "{\"A234567890123456789012345678901\": false, \"KeyInt\": -1234,\"KeyUint: 1353214,\"KeyString\": 123456789012345,\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";
static const char json_no_brak[] =
    "{\"KeyBool\": true,\"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": \"foobar\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322}";
static const char json_no_start[] =
    "\"KeyBool\": true,\"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": \"foobar\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";

/* now test the decode on a string */
TEST_CASE_SELF(test_json_decode_errors)
{
    int rc;

    rc = json_test_decode(json_ok1);
    TEST_ASSERT(rc == 0);
    rc = json_test_decode(json_ok2);
    TEST_ASSERT(rc == 0);
    rc = json_test_decode(json_ok3);
    TEST_ASSERT(rc == 0);
    rc = json_test_decode(json_int_out_of_range1);
    TEST_ASSERT(rc == 0);
    rc = json_test_decode(json_int_out_of_range2);
    TEST_ASSERT(rc == 0);
    /* Invalid bool value, not "true" or "false", makes false */
    rc = json_test_decode(json_bool_fail);
    TEST_ASSERT(rc == 0 && bool_val == false);
    rc = json_test_decode(json_missing_val);
    TEST_ASSERT(rc != 0);
    rc = json_test_decode(json_missing_quote);
    TEST_ASSERT(rc != 0);
    rc = json_test_decode(json_str_to_long1);
    TEST_ASSERT(rc == JSON_ERR_STRLONG);
    rc = json_test_decode(json_str_to_long2);
    TEST_ASSERT(rc == JSON_ERR_STRLONG);
    rc = json_test_decode(json_str_no_quotes);
    TEST_ASSERT(rc != 0);
    rc = json_test_decode(json_extra_coma0);
    TEST_ASSERT(rc != 0);
    rc = json_test_decode(json_extra_coma1);
    TEST_ASSERT(rc != 0);
    rc = json_test_decode(json_extra_coma2);
    TEST_ASSERT(rc != 0);
    rc = json_test_decode(json_attr_to_long);
    TEST_ASSERT(rc == JSON_ERR_ATTRLEN);
    rc = json_test_decode(json_bad_attr);
    TEST_ASSERT(rc == JSON_ERR_BADATTR);
    rc = json_test_decode(json_no_brak);
    TEST_ASSERT(rc != 0);
    rc = json_test_decode(json_no_start);
    TEST_ASSERT(rc != 0);
}
