/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _UTILS_QUEUE_H_
#define _UTILS_QUEUE_H_

#include "osdp/list.h"

#define queue_node_t node_t

typedef struct {
    list_t list;
} queue_t;

void queue_init(queue_t *queue);
void queue_enqueue(queue_t *queue, queue_node_t *node);
int queue_dequeue(queue_t *queue, queue_node_t **node);
int queue_peek_last(queue_t *queue, queue_node_t **node);
int queue_peek_first(queue_t *queue, queue_node_t **node);

#endif /* _UTILS_QUEUE_H_ */
