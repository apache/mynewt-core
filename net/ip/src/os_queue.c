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
#include <limits.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>

#include "os/mynewt.h"
#include <ip/os_queue.h>

/*
 * XXX temporary, until lwip main loop runs via eventq.
 */
int
os_queue_init(struct os_queue *q, uint8_t elem_size, uint8_t elem_cnt)
{
    assert(elem_cnt < SHRT_MAX);

    q->oq_q = malloc(elem_size * elem_cnt);
    if (!q->oq_q) {
        return -1;
    }
    q->oq_head = q->oq_tail = 0;
    q->oq_size = elem_cnt;
    q->oq_elem_size = elem_size;
    os_sem_init(&q->oq_space, elem_cnt);
    os_sem_init(&q->oq_items, 0);
    return 0;
}

int
os_queue_put(struct os_queue *q, void *elem, uint32_t timeout)
{
    int rc;
    int sr;
    uint8_t *ptr;

    rc = os_sem_pend(&q->oq_space, timeout);
    if (rc) {
        return rc;
    }
    OS_ENTER_CRITICAL(sr);
    ptr = q->oq_q;
    ptr += q->oq_elem_size * q->oq_head;
    memcpy(ptr, elem, q->oq_elem_size);
    q->oq_head++;
    if (q->oq_head >= q->oq_size) {
        q->oq_head = 0;
    }
    OS_EXIT_CRITICAL(sr);
    os_sem_release(&q->oq_items);
    return 0;
}

int
os_queue_get(struct os_queue *q, void *elem, uint32_t timeout)
{
    int rc;
    int sr;
    uint8_t *ptr;

    rc = os_sem_pend(&q->oq_items, timeout);
    if (rc) {
        return rc;
    }
    OS_ENTER_CRITICAL(sr);
    ptr = q->oq_q;
    ptr += q->oq_elem_size * q->oq_tail;
    memcpy(elem, ptr, q->oq_elem_size);
    q->oq_tail++;
    if (q->oq_tail >= q->oq_size) {
        q->oq_tail = 0;
    }
    OS_EXIT_CRITICAL(sr);
    os_sem_release(&q->oq_space);
    return 0;
}
