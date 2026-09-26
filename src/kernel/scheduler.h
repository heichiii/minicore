#ifndef MINICORE_KERNEL_SCHEDULER_H
#define MINICORE_KERNEL_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

#include "list.h"

typedef void (*thread_entry)(void *argument);

bool scheduler_init(uint64_t timebase_frequency);
bool thread_create(const char *name, thread_entry entry, void *argument);
void scheduler_yield(void);
void scheduler_sleep(uint64_t ticks);
uint64_t scheduler_ticks(void);
uint64_t scheduler_preemptions(void);
void scheduler_tick(void);
bool scheduler_timer_active(void);
void scheduler_stop_timer(void);
void scheduler_reap(void);

/* Synchronization back-end.  Callers hold interrupts disabled. */
void scheduler_block_current(struct list_node *queue);
void scheduler_wake_node(struct list_node *node);
void scheduler_switch(void);

bool scheduler_register_probe(unsigned iterations);

#endif
