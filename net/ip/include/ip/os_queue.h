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
#ifndef __OS_QUEUE_H__
#define __OS_QUEUE_H__

#ifdef __cplusplus
extern "C" {
#endif

struct os_queue {
    uint8_t oq_head;
    uint8_t oq_tail;
    uint8_t oq_size;
    uint8_t oq_elem_size;
    struct os_sem oq_items;
    struct os_sem oq_space;
    void *oq_q;
};

int os_queue_init(struct os_queue *, uint8_t elem_size, uint8_t elem_cnt);
int os_queue_put(struct os_queue *, void *elem, uint32_t timeout);
int os_queue_get(struct os_queue *, void *elem, uint32_t timeout);
void os_queue_free(struct os_queue *);

#ifdef __cplusplus
}
#endif

#endif /* __OS_QUEUE_H__ */
