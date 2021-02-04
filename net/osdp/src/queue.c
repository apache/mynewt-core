/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include "osdp/queue.h"

void
queue_init(queue_t *queue)
{
    list_init(&queue->list);
}

void
queue_enqueue(queue_t *queue, queue_node_t *node)
{
    list_append(&queue->list, node);
}

int
queue_dequeue(queue_t *queue, queue_node_t **node)
{
    return list_popleft(&queue->list, node);
}

int
queue_peek_last(queue_t *queue, queue_node_t **node)
{
    if (queue->list.tail == NULL) {
        return -1;
    }

    *node = queue->list.tail;
    return 0;
}

int
queue_peek_first(queue_t *queue, queue_node_t **node)
{
    if (queue->list.head == NULL) {
        return -1;
    }

    *node = queue->list.head;
    return 0;
}
