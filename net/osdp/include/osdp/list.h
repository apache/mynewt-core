/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _UTILS_LIST_H_
#define _UTILS_LIST_H_

#include <stddef.h>

typedef struct node_s node_t;

struct node_s {
    node_t *next;
    node_t *prev;
};

struct list_s {
    node_t *head;
    node_t *tail;
};

typedef struct list_s list_t;

#define OSDP_LIST_FOREACH(list, node) \
    for (node_t *node = (list)->head; node != NULL; node = node->next)

void list_init(list_t *list);
void list_append(list_t *list, node_t *node);
void list_appendleft(list_t *list, node_t *node);
int list_pop(list_t *list, node_t **node);
int list_popleft(list_t *list, node_t **node);

void list_remove_node(list_t *list, node_t *node);
int list_remove_nodes(list_t *list, node_t *start, node_t *end);
void list_insert_node(list_t *list, node_t *after, node_t *new);
int list_insert_nodes(list_t *list, node_t *after, node_t *start, node_t *end);

/*--- singly-linked list ---*/

typedef struct snode_s snode_t;

struct snode_s {
    snode_t *next;
};

struct slist_s {
    snode_t *head;
};

typedef struct slist_s slist_t;

void slist_init(slist_t *list);
void slist_append(slist_t *list, snode_t *after, snode_t *node);
void slist_appendleft(slist_t *list, snode_t *node);
int slist_pop(slist_t *list, snode_t *after, snode_t **node);
int slist_popleft(slist_t *list, snode_t **node);

int slist_remove_node(slist_t *list, snode_t *node);
void slist_insert_node(slist_t *list, snode_t *after, snode_t *new);

#endif /* _UTILS_LIST_H_ */
