/*
 * Copyright (c) 2020 Siddharth Chandrasekaran <siddharth@embedjournal.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdbool.h>

#include "osdp/list.h"

void
list_init(list_t *list)
{
    list->head = NULL;
    list->tail = NULL;
}

void
list_append(list_t *list, node_t *node)
{
    node->prev = list->tail;
    node->next = NULL;
    if (list->tail) {
        list->tail->next = node;
    }
    list->tail = node;
    if (!list->head) {
        list->head = node;
    }
}

void
list_appendleft(list_t *list, node_t *node)
{
    node->prev = NULL;
    node->next = list->head;
    if (list->head) {
        list->head->prev = node;
    }
    list->head = node;
    if (!list->tail) {
        list->tail = node;
    }
}

int
list_pop(list_t *list, node_t **node)
{
    if (!list->tail) {
        return -1;
    }
    *node = list->tail;
    list->tail = list->tail->prev;
    if (list->tail) {
        list->tail->next = NULL;
    } else {
        list->head = NULL;
    }
    return 0;
}

int
list_popleft(list_t *list, node_t **node)
{
    if (!list->head) {
        return -1;
    }
    *node = list->head;
    list->head = list->head->next;
    if (list->head) {
        list->head->prev = NULL;
    } else {
        list->tail = NULL;
    }
    return 0;
}

void
list_remove_node(list_t *list, node_t *node)
{
    if (node->prev == NULL) {
        /* remove head */
        list->head = node->next;
        if (list->head == NULL) {
            list->tail = NULL;
        } else {
            list->head->prev = NULL;
        }
    } else if (node->next == NULL) {
        /* remove tail */
        list->tail = node->prev;
        if (list->tail == NULL) {
            list->head = NULL;
        } else {
            list->tail->next = NULL;
        }
    } else {
        /* remove in-between */
        node->next->prev = node->prev;
        node->prev->next = node->next;
    }
}

node_t *
list_find_node(list_t *list, node_t *node)
{
    node_t *p;

    p = list->head;
    while (p && p != node) {
        p = p->next;
    }
    return p;
}

bool
list_check_links(node_t *p1, node_t *p2)
{
    node_t *p1_prev, *p2_next;

    if (p1 == NULL || p2 == NULL) {
        return false;
    }

    if (p1 == p2) {
        return true;
    }

    p1_prev = p1->prev;
    p2_next = p2->next;

    while (p1 && p2 && p1 != p2 && p1->next != p2->prev &&
           p1->prev == p1_prev && p2->next == p2_next) {
        p1_prev = p1;
        p1 = p1->next;

        p2_next = p2;
        p2 = p2->prev;
    }

    return (p1 && p2) && (p1 == p2 || p1->next == p2->prev);
}

int
list_remove_nodes(list_t *list, node_t *start, node_t *end)
{
    /* check if start in list and the range [start:end] is a valid list  */
    if (list_find_node(list, start) == NULL ||
        list_check_links(start, end) == 0) {
        return -1;
    }

    if (start->prev == NULL) {
        /* remove from head */
        list->head = end->next;
        if (list->head == NULL) {
            list->head = NULL;
        } else {
            list->head->prev = NULL;
        }
    } else if (end->next == NULL) {
        /* remove upto tail */
        start->prev->next = NULL;
        list->tail = start->prev;
    } else {
        /* remove in-between */
        start->prev->next = end->next;
        end->next->prev = start->prev;
    }

    return 0;
}

void
list_insert_node(list_t *list, node_t *after, node_t *new)
{
    node_t *next;

    if (after == NULL) {
        /* insert at head */
        next = list->head;
        list->head = new;
    } else {
        next = after->next;
        after->next = new;
    }
    new->prev = after;
    new->next = next;
    next->prev = new;
}

int
list_insert_nodes(list_t *list, node_t *after, node_t *start, node_t *end)
{
    node_t *next;

    /* check if nodes is a valid list */
    if (list_check_links(start, end) == 0) {
        return -1;
    }

    if (list->head == NULL) {
        /* If list is empty, the nodes becomes the list */
        list->head = start;
        list->tail = end;
    } else if (after == NULL) {
        /* if 'after' node is not given prepend the nodes to list */
        end->next = list->head;
        list->head = start;
        list->head->prev = NULL;
    } else {
        /* insert nodes into list after the 'after' node */
        next = after->next;
        after->next = start;
        start->prev = after;
        end->next = next;
        if (next != NULL) {
            next->prev = end;
        } else {
            list->tail = end;
        }
    }

    return 0;
}

/*--- singly-linked list ---*/

void
slist_init(slist_t *list)
{
    list->head = NULL;
}

void
slist_append(slist_t *list, snode_t *after, snode_t *node)
{
    snode_t *p;

    p = after ? after : list->head;
    if (p == NULL) {
        list->head = node;
    } else {
        while (p && p->next) {
            p = p->next;
        }
        p->next = node;
    }
    node->next = NULL;
}

void
slist_appendleft(slist_t *list, snode_t *node)
{
    node->next = list->head;
    list->head = node;
}

int
slist_pop(slist_t *list, snode_t *after, snode_t **node)
{
    snode_t *n1, *n2;

    if (list->head == NULL) {
        return -1;
    }
    if (list->head->next == NULL) {
        *node = list->head;
        list->head = NULL;
    } else {
        n1 = after ? after : list->head;
        n2 = n1->next;
        while (n2 && n2->next) {
            n1 = n1->next;
            n2 = n2->next;
        }
        n1->next = NULL;
        *node = n2;
    }
    return 0;
}

int
slist_popleft(slist_t *list, snode_t **node)
{
    if (list->head == NULL) {
        return -1;
    }

    *node = list->head;
    list->head = list->head->next;
    return 0;
}

int
slist_remove_node(slist_t *list, snode_t *node)
{
    snode_t *prev = NULL, *cur;

    cur = list->head;
    while (cur && cur != node) {
        prev = cur;
        cur = cur->next;
    }
    if (cur == NULL) {
        return -1;
    }
    if (prev == NULL) {
        list->head = cur->next;
    } else {
        prev->next = cur->next;
    }
    return 0;
}

void
slist_insert_node(slist_t *list, snode_t *after, snode_t *new)
{
    if (after == NULL) {
        /* same as append left */
        new->next = list->head;
        list->head = new;
    } else {
        /* assert after in list here? */
        new->next = after->next;
        after->next = new;
    }
}
