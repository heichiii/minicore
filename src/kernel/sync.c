#include "sync.h"

#include "scheduler.h"
#include "../arch/riscv/csr.h"

uint64_t irq_save(void)
{
    uint64_t status = csr_read_sstatus();

    csr_clear_sstatus(SSTATUS_SIE);
    return status;
}

void irq_restore(uint64_t flags)
{
    if ((flags & SSTATUS_SIE) != 0)
        csr_set_sstatus(SSTATUS_SIE);
    else
        csr_clear_sstatus(SSTATUS_SIE);
}

bool irq_enabled(void)
{
    return (csr_read_sstatus() & SSTATUS_SIE) != 0;
}

void spinlock_init(struct spinlock *lock)
{
    lock->locked = 0;
}

void spin_lock(struct spinlock *lock)
{
    uint32_t value;

    do {
        __asm__ volatile("amoswap.w.aq %0, %2, (%1)"
                         : "=r"(value)
                         : "r"(&lock->locked), "r"(1U)
                         : "memory");
    } while (value != 0);
}

void spin_unlock(struct spinlock *lock)
{
    __asm__ volatile("fence rw, w" : : : "memory");
    lock->locked = 0;
}

void wait_queue_init(struct wait_queue *queue)
{
    list_init(&queue->sleepers);
}

void wait_queue_sleep(struct wait_queue *queue, struct spinlock *lock)
{
    scheduler_block_current(&queue->sleepers);
    spin_unlock(lock);
    scheduler_switch();
    spin_lock(lock);
}

bool wait_queue_wake_one(struct wait_queue *queue)
{
    if (list_empty(&queue->sleepers))
        return false;
    scheduler_wake_node(queue->sleepers.next);
    return true;
}

void wait_queue_wake_all(struct wait_queue *queue)
{
    while (wait_queue_wake_one(queue))
        ;
}
