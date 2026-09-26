#include "scheduler.h"

#include "sync.h"
#include "process.h"
#include "../arch/riscv/trap.h"
#include "../arch/riscv/csr.h"
#include "../arch/riscv/sbi.h"
#include "../mm/heap.h"
#include "../mm/layout.h"
#include "../mm/page_alloc.h"
#include "../runtime.h"

enum thread_state {
    THREAD_RUNNING,
    THREAD_RUNNABLE,
    THREAD_SLEEPING,
    THREAD_DEAD,
};

struct context {
    uint64_t ra;
    uint64_t sp;
    uint64_t saved[12];
};

struct thread {
    struct context context;
    struct process *process;
    struct trap_frame *user_frame;
    struct list_node all_link;
    struct list_node wait_link;
    enum thread_state state;
    thread_entry entry;
    void *argument;
    uint64_t stack_page;
    uint64_t wake_tick;
    const char *name;
    bool idle;
};

extern void context_switch(struct context *old, struct context *next);

static LIST_HEAD(all_threads);
static struct thread bootstrap;
static struct thread *current;
static struct thread *idle_thread;
static uint64_t tick_count;
static uint64_t preemption_count;
static uint64_t timer_interval;
static bool timer_running;
static const struct page_table *kernel_page_table;

extern _Noreturn void user_thread_enter(void);

struct process *current_process(void)
{
    return current ? current->process : 0;
}

static void thread_trampoline(void)
{
    csr_set_sstatus(SSTATUS_SIE);
    current->entry(current->argument);
    thread_exit();
}

static void idle_main(void *argument)
{
    (void)argument;
    for (;;) {
        scheduler_reap();
        __asm__ volatile("wfi");
        scheduler_yield();
    }
}

static struct thread *pick_next(void)
{
    struct list_node *start = current->all_link.next;
    struct list_node *node = start;

    do {
        if (node == &all_threads)
            node = node->next;
        if (node != &all_threads) {
            struct thread *thread = container_of(node, struct thread,
                                                  all_link);

            if (thread->state == THREAD_RUNNABLE && !thread->idle)
                return thread;
            node = node->next;
        }
    } while (node != start);
    if (current->state == THREAD_RUNNABLE && !current->idle)
        return current;
    return idle_thread;
}

void scheduler_switch(void)
{
    struct thread *previous = current;
    struct thread *next = pick_next();

    if (!next || next == previous) {
        if (previous->state == THREAD_RUNNABLE)
            previous->state = THREAD_RUNNING;
        return;
    }
    next->state = THREAD_RUNNING;
    current = next;
    vm_activate(next->process ? &next->process->as.table : kernel_page_table);
    context_switch(&previous->context, &next->context);
}

static bool create_thread(const char *name, thread_entry entry,
                          void *argument, bool idle,
                          struct process *process,
                          const struct trap_frame *initial)
{
    struct thread *thread = kmalloc(sizeof(*thread));
    uint64_t stack;

    if (!thread)
        return false;
    stack = page_alloc();
    if (!stack) {
        kfree(thread);
        return false;
    }
    memset(thread, 0, sizeof(*thread));
    list_init(&thread->all_link);
    list_init(&thread->wait_link);
    thread->state = THREAD_RUNNABLE;
    thread->entry = entry;
    thread->argument = argument;
    thread->stack_page = stack;
    thread->name = name;
    thread->idle = idle;
    thread->process = process;
    thread->context.ra = (uintptr_t)thread_trampoline;
    thread->context.sp = (uintptr_t)phys_to_virt(stack) + PAGE_SIZE;
    if (process) {
        thread->user_frame = (struct trap_frame *)(uintptr_t)
            (thread->context.sp - sizeof(struct trap_frame));
        *thread->user_frame = *initial;
        thread->user_frame->sstatus = SSTATUS_SPIE;
        thread->context.sp = (uintptr_t)thread->user_frame;
        thread->context.ra = (uintptr_t)user_thread_enter;
    }
    list_push_back(&all_threads, &thread->all_link);
    if (idle)
        idle_thread = thread;
    return true;
}

bool scheduler_init(uint64_t timebase_frequency,
                    const struct page_table *kernel_table)
{
    if (!kernel_table || !kernel_table->root)
        return false;
    kernel_page_table = kernel_table;
    memset(&bootstrap, 0, sizeof(bootstrap));
    list_init(&all_threads);
    list_init(&bootstrap.all_link);
    list_init(&bootstrap.wait_link);
    bootstrap.state = THREAD_RUNNING;
    bootstrap.name = "bootstrap";
    current = &bootstrap;
    list_push_back(&all_threads, &bootstrap.all_link);
    tick_count = 0;
    preemption_count = 0;
    timer_interval = timebase_frequency / 200;
    if (timer_interval == 0)
        timer_interval = 1;
    if (!create_thread("idle", idle_main, 0, true, 0, 0))
        return false;
    timer_running = true;
    sbi_set_timer(csr_read_time() + timer_interval);
    csr_set_sie(SIE_STIE);
    csr_set_sstatus(SSTATUS_SIE);
    return true;
}

bool thread_create(const char *name, thread_entry entry, void *argument)
{
    uint64_t flags;
    bool result;

    if (!entry)
        return false;
    flags = irq_save();
    result = create_thread(name, entry, argument, false, 0, 0);
    irq_restore(flags);
    return result;
}

bool thread_create_user(struct process *process,
                        const struct trap_frame *initial)
{
    uint64_t flags = irq_save();
    bool result = process && initial &&
        create_thread("user", 0, 0, false, process, initial);

    irq_restore(flags);
    return result;
}

void scheduler_yield(void)
{
    uint64_t flags = irq_save();

    if (current->state == THREAD_RUNNING)
        current->state = THREAD_RUNNABLE;
    scheduler_switch();
    irq_restore(flags);
}

void scheduler_sleep(uint64_t ticks)
{
    uint64_t flags;

    if (ticks == 0) {
        scheduler_yield();
        return;
    }
    flags = irq_save();
    current->wake_tick = tick_count + ticks;
    current->state = THREAD_SLEEPING;
    scheduler_switch();
    irq_restore(flags);
}

void scheduler_block_current(struct list_node *queue)
{
    current->state = THREAD_SLEEPING;
    list_push_back(queue, &current->wait_link);
}

void scheduler_wake_node(struct list_node *node)
{
    struct thread *thread = container_of(node, struct thread, wait_link);

    list_remove(node);
    if (thread->state == THREAD_SLEEPING)
        thread->state = THREAD_RUNNABLE;
}

void scheduler_tick(void)
{
    ++tick_count;
    for (struct list_node *node = all_threads.next; node != &all_threads;
         node = node->next) {
        struct thread *thread = container_of(node, struct thread, all_link);

        if (thread->state == THREAD_SLEEPING && thread->wake_tick != 0 &&
            thread->wake_tick <= tick_count) {
            thread->wake_tick = 0;
            thread->state = THREAD_RUNNABLE;
        }
    }
    if (timer_running)
        sbi_set_timer(csr_read_time() + timer_interval);
    if (current->state == THREAD_RUNNING) {
        current->state = THREAD_RUNNABLE;
        ++preemption_count;
    }
    scheduler_switch();
}

bool scheduler_timer_active(void)
{
    return timer_running;
}

uint64_t scheduler_ticks(void)
{
    return tick_count;
}

uint64_t scheduler_preemptions(void)
{
    return preemption_count;
}

void scheduler_stop_timer(void)
{
    uint64_t flags = irq_save();

    timer_running = false;
    csr_clear_sie(SIE_STIE);
    sbi_set_timer(UINT64_MAX);
    irq_restore(flags);
}

_Noreturn void thread_exit(void)
{
    irq_save();
    current->state = THREAD_DEAD;
    scheduler_switch();
    for (;;)
        __asm__ volatile("wfi");
}

void scheduler_reap(void)
{
    uint64_t flags = irq_save();
    struct list_node *node = all_threads.next;

    while (node != &all_threads) {
        struct thread *thread = container_of(node, struct thread, all_link);

        node = node->next;
        if (thread != current && !thread->idle &&
            thread->state == THREAD_DEAD) {
            list_remove(&thread->all_link);
            page_free(thread->stack_page);
            if (thread->process)
                process_reap(thread->process);
            kfree(thread);
        }
    }
    irq_restore(flags);
}
