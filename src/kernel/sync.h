#ifndef MINICORE_KERNEL_SYNC_H
#define MINICORE_KERNEL_SYNC_H

#include <stdbool.h>
#include <stdint.h>

#include "list.h"

struct spinlock {
    volatile uint32_t locked;
};

struct wait_queue {
    struct list_node sleepers;
};

#define SPINLOCK_INITIALIZER { 0 }

uint64_t irq_save(void);
void irq_restore(uint64_t flags);
bool irq_enabled(void);
void spinlock_init(struct spinlock *lock);
void spin_lock(struct spinlock *lock);
void spin_unlock(struct spinlock *lock);
void wait_queue_init(struct wait_queue *queue);
void wait_queue_sleep(struct wait_queue *queue, struct spinlock *lock);
bool wait_queue_wake_one(struct wait_queue *queue);
void wait_queue_wake_all(struct wait_queue *queue);

#endif
