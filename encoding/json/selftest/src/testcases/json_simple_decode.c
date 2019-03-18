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

/* now test the decode on a string */
TEST_CASE_SELF(test_json_simple_decode)
{
    struct test_jbuf tjb;
    struct test_jbuf tjb1;
    struct test_jbuf tjbboolspacearr;
    struct test_jbuf tjbboolemptyarr;
    long long unsigned int uint_val;
    long long int int_val;
    bool bool_val;
    char string1[16];
    char string2[16];
    long long int intarr[8];
    int rc;
    int rc1;
    int rcbsa;
    int array_count;
    int array_countemp;
    bool boolarr[2];
    unsigned long long uintarr[5];
    int array_count1;
    int array_count1u;
    bool boolspacearr[3];
    bool boolemptyarr[2];

    struct json_attr_t test_attr[7] = {
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

    test_buf_init(&tjb, output);

    rc = json_read_object(&tjb.json_buf, test_attr);
    TEST_ASSERT(rc==0);
    TEST_ASSERT(bool_val == 1);
    TEST_ASSERT(int_val ==  -1234);
    TEST_ASSERT(uint_val == 1353214);

    rc = memcmp(string1, "foobar", strlen("foobar"));
    TEST_ASSERT(rc==0);

    rc = memcmp(string2, "foobarlongstring", 10);
    TEST_ASSERT(rc==0);

    TEST_ASSERT(array_count == 3);
    TEST_ASSERT(intarr[0] == 153);
    TEST_ASSERT(intarr[1] == 2532);
    TEST_ASSERT(intarr[2] == -322);

    /*testing for the boolean*/
    struct json_attr_t test_attr1[] = {
        [0] = {
            .attribute = "KeyBoolArr",
            .type = t_array,
            .addr.array = {
                .element_type = t_boolean,
                .arr.booleans.store = boolarr,
                .maxlen = sizeof boolarr / sizeof boolarr[0],
                .count =&array_count1,
            },
            .nodefault = true,
            .len = sizeof( boolarr),
        },

        [1] = {
            .attribute = "KeyUintArr",
            .type = t_array,
            .addr.array = {
                .element_type = t_uinteger,
                .arr.uintegers.store = uintarr,
                .maxlen = sizeof uintarr / sizeof uintarr[0],
                .count =&array_count1u,
            },
            .nodefault = true,
            .len = sizeof( uintarr),
        },

        [2] = { 0 },
   };

   test_buf_init(&tjb1, output1);

   rc1 = json_read_object(&tjb1.json_buf, test_attr1);
   TEST_ASSERT(rc1==0);

   TEST_ASSERT(boolarr[0] == true);
   TEST_ASSERT(boolarr[1] == false);

   TEST_ASSERT(uintarr[0] == 0);
   TEST_ASSERT(uintarr[1] == 65535);
   TEST_ASSERT(uintarr[2] == 4294967295ULL);
   TEST_ASSERT(uintarr[3] == 8589934590ULL);
   TEST_ASSERT(uintarr[4] == 3451257ULL);

    /*testing arrays with empty spaces within the elements*/
   struct json_attr_t test_boolspacearr[] = {
       [0] = {
           .attribute = "KeyBoolArr",
           .type = t_array,
           .addr.array = {
               .element_type = t_boolean,
               .arr.booleans.store = boolspacearr,
               .maxlen = sizeof boolspacearr / sizeof boolspacearr[0],
               .count =&array_count1,
           },
           .nodefault = true,
           .len = sizeof( boolspacearr),
       },

       [1] = { 0 },
   };

    test_buf_init(&tjbboolspacearr, outputboolspace);

    rcbsa = json_read_object(&tjbboolspacearr.json_buf, test_boolspacearr);
    TEST_ASSERT(rcbsa == 0);

    TEST_ASSERT(boolspacearr[0] == true);
    TEST_ASSERT(boolspacearr[1] == false);
    TEST_ASSERT(boolspacearr[2] == true);

    /*testing array with empty value*/
    struct json_attr_t test_boolemptyarr[] = {
        [0] = {
            .attribute = "KeyBoolArr",
           .type = t_array,
           .addr.array = {
               .element_type = t_boolean,
               .arr.booleans.store = boolemptyarr,
               .maxlen = sizeof boolemptyarr / sizeof boolemptyarr[0],
               .count =&array_countemp,
           },
           .nodefault = true,
           .len = sizeof( boolemptyarr),
        },

        [1] = { 0 },
    };

   test_buf_init(&tjbboolemptyarr, outputboolempty);

   rcbsa = json_read_object(&tjbboolemptyarr.json_buf, test_boolemptyarr);
   TEST_ASSERT(rcbsa == 6);
}
