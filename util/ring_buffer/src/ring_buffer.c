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

#include <stdint.h>
#include <util/ring_buffer.h>
#include <os/os.h>

static int
ring_buffer_inc_and_wrap(int i, int max)
{
    return (i + 1) & (max - 1);
}

void
ring_buffer_init(struct ring_buffer *rb, uint8_t *buf, int size)
{
    rb->head = 0;
    rb->tail = 0;
    rb->size = size;
    rb->buf = buf;
}

void
ring_buffer_push(struct ring_buffer *rb, uint8_t ch)
{
    rb->buf[rb->head] = ch;
    rb->head = ring_buffer_inc_and_wrap(rb->head, rb->size);
}

uint8_t
ring_buffer_pull(struct ring_buffer *rb)
{
    uint8_t ch;

    ch = rb->buf[rb->tail];
    rb->tail = ring_buffer_inc_and_wrap(rb->tail, rb->size);

    return ch;
}

bool
ring_buffer_is_full(const struct ring_buffer *rb)
{
    return ring_buffer_inc_and_wrap(rb->head, rb->size) == rb->tail;
}

bool
ring_buffer_is_empty(const struct ring_buffer *rb)
{
    return rb->head == rb->tail;
}

int
ring_buffer_free_space(const struct ring_buffer *rb)
{
    uint32_t head = rb->head;
    uint32_t tail = rb->tail;

    return rb->size - ((tail - head) & (rb->size - 1)) - 1;
}

int
ring_buffer_data_count(const struct ring_buffer *rb)
{
    uint32_t head = rb->head;
    uint32_t tail = rb->tail;

    return (head - tail) & (rb->size - 1);
}

int
ring_buffer_write(struct ring_buffer *rb, const uint8_t *data, int len)
{
    int free_space = ring_buffer_free_space(rb);
    int n = min(free_space, len);

    for (int i = 0; i < n; i++) {
        ring_buffer_push(rb, data[i]);
    }
    return n;
}

int
ring_buffer_read(struct ring_buffer *rb, uint8_t *data, int len)
{
    int data_len = ring_buffer_data_count(rb);

    if (data_len > len) {
        data_len = len;
    }

    for (int i = 0; i < data_len; i++) {
        data[i] = ring_buffer_pull(rb);
    }
    return data_len;
}

int
ring_buffer_peek(struct ring_buffer *rb, uint8_t *data, int len)
{
    int data_len = ring_buffer_data_count(rb);

    if (data_len > len) {
        data_len = len;
    }

    for (int i = 0, j = rb->tail; i < data_len; i++) {
        data[i] = rb->buf[j];
        j = ring_buffer_inc_and_wrap(j, rb->size);
    }

    return data_len;
}

int
ring_buffer_peek_byte(struct ring_buffer *rb)
{
    int data_len = ring_buffer_data_count(rb);
    if (data_len > 0) {
        return rb->buf[rb->tail];
    } else {
        return -1;
    }
}
