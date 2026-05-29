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

static int
ring_buffer_inc_and_wrap(int i, int max)
{
    return (i + 1) & (max - 1);
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
