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

#ifndef _UTIL_RING_BUFFER_H_
#define _UTIL_RING_BUFFER_H_

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ring_buffer {
    /* Write pointer */
    uint8_t head;
    /* Read pointer */
    uint8_t tail;
    /** Ring buffer size, must be power of 2 */
    uint16_t size;
    /** Data buffer */
    uint8_t *buf;
};

/**
 * Push new element to ring buffer.
 * Ring buffer must have at least one empty space.
 * ring_buffer_is_full should be called before pushing element into
 * buffer.
 *
 * @param rb ring buffer
 * @param ch element to add to ring buffer
 */
void ring_buffer_push(struct ring_buffer *rb, uint8_t ch);

/**
 * Pull oldest element from ring buffer.
 * Ring buffer can not be empty.
 * Function ring_buffer_is_empty() should be called before pulling
 * element from the buffer.
 *
 * @param rb ring buffer
 * @return oldest emenet in ring buffer
 */
uint8_t ring_buffer_pull(struct ring_buffer *rb);

/**
 * Check if ring buffer is full.
 *
 * @param rb ring buffer
 * @return true if ring buffer is full
 */
bool ring_buffer_is_full(const struct ring_buffer *rb);

/**
 * Check if ring buffer is empty.
 *
 * @param rb ring buffer
 * @return true if ring buffer is empty
 */
bool ring_buffer_is_empty(const struct ring_buffer *rb);

#ifdef __cplusplus
}
#endif

#endif /* _UTIL_RING_BUFFER_H_ */
