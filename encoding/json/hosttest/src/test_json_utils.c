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
#include <assert.h>
#include <string.h>
#include "testutil/testutil.h"
#include "test_json_priv.h"
#include "json/json.h"

char *output = "{\"KeyBool\": true,\"KeyInt\": -1234,\"KeyUint\": 1353214,\"KeyString\": \"foobar\",\"KeyStringN\": \"foobarlong\",\"KeyIntArr\": [153,2532,-322]}";

char *output1 ="{\"KeyBoolArr\": [true, false], \"KeyUintArr\": [0, 65535, 4294967295, 8589934590, 3451257]}";
char *outputboolspace = "{\"KeyBoolArr\": [    true    ,    false,true         ]}";
char *outputboolempty = "{\"KeyBoolArr\": , \"KeyBoolArr\": [  ]}";

char *bigbuf;
int buf_index;

int
test_write(void *buf, char* data, int len)
{
    int i;

    for (i = 0; i < len; i++) {
        bigbuf[buf_index++] = data[i];
    }

    return len;
}

char
test_jbuf_read_next(struct json_buffer *jb)
{
    char c;
    struct test_jbuf  *ptjb = (struct test_jbuf*) jb;

    if ((ptjb->start_buf + ptjb->current_position) <= ptjb->end_buf) {
        c = *(ptjb->start_buf + ptjb->current_position);
        ptjb->current_position++;
        return c;
    }

    return '\0';
}

/* this goes backward in the buffer one character */
char
test_jbuf_read_prev(struct json_buffer *jb)
{
    char c;
    struct test_jbuf  *ptjb = (struct test_jbuf*) jb;

    if (ptjb->current_position) {
       ptjb->current_position--;
       c = *(ptjb->start_buf + ptjb->current_position);
       return c;
    }

    /* can't rewind */
    return '\0';
}

int
test_jbuf_readn(struct json_buffer *jb, char *buf, int size)
{
    struct test_jbuf  *ptjb = (struct test_jbuf*) jb;

    int remlen;

    remlen = ptjb->end_buf - (ptjb->start_buf + ptjb->current_position);
    if (size > remlen) {
        size = remlen;
    }

    memcpy(buf, ptjb->start_buf + ptjb->current_position, size);
    ptjb->current_position += size;
    return size;
}

void
test_buf_init(struct test_jbuf *ptjb, char *string)
{
    /* initialize the decode */
    ptjb->json_buf.jb_read_next = test_jbuf_read_next;
    ptjb->json_buf.jb_read_prev = test_jbuf_read_prev;
    ptjb->json_buf.jb_readn = test_jbuf_readn;
    ptjb->start_buf = string;
    ptjb->end_buf = string + strlen(string);
    /* end buf points to the NULL */
    ptjb->current_position = 0;
}
