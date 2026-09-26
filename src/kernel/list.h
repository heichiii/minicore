#ifndef MINICORE_KERNEL_LIST_H
#define MINICORE_KERNEL_LIST_H

#include <stdbool.h>
#include <stddef.h>

struct list_node {
    struct list_node *prev;
    struct list_node *next;
};

#define LIST_HEAD(name) struct list_node name = { &(name), &(name) }
#define container_of(pointer, type, member) \
    ((type *)((char *)(pointer) - offsetof(type, member)))

static inline void list_init(struct list_node *head)
{
    head->prev = head;
    head->next = head;
}

static inline bool list_empty(const struct list_node *head)
{
    return head->next == head;
}

static inline void list_insert_before(struct list_node *position,
                                      struct list_node *node)
{
    node->prev = position->prev;
    node->next = position;
    position->prev->next = node;
    position->prev = node;
}

static inline void list_push_back(struct list_node *head,
                                  struct list_node *node)
{
    list_insert_before(head, node);
}

static inline void list_remove(struct list_node *node)
{
    node->prev->next = node->next;
    node->next->prev = node->prev;
    list_init(node);
}

#endif
