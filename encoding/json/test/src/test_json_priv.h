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
#ifndef TEST_JSON_H
#define TEST_JSON_H

#include <assert.h>
#include <string.h>
#include "testutil/testutil.h"
#include "json/json.h"

#ifdef __cplusplus
extern "C" {
#endif

char *output;
char *output1;
char *outputboolspace;
char *outputboolempty;

#define JSON_BIGBUF_SIZE    192
char *bigbuf;
int buf_index;

/* a test structure to hold the json flat buffer and pass bytes
 * to the decoder */
struct test_jbuf {
    /* json_buffer must be first element in the structure */
    struct json_buffer json_buf;
    char * start_buf;
    char * end_buf;
    int current_position;
};

char test_jbuf_read_next(struct json_buffer *jb);
char test_jbuf_read_prev(struct json_buffer *jb);
int test_jbuf_readn(struct json_buffer *jb, char *buf, int size);
int test_write(void *buf, char* data, int len);
void test_buf_init(struct test_jbuf *ptjb, char *string);

#ifdef __cplusplus
}
#endif

#endif /* TEST_JSON_H */
