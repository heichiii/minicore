#include "process.h"
#include "scheduler.h"
#include "../mm/heap.h"

/*
 * Allocate the process container and its empty address space.  No scheduler
 * state refers to the object yet, so failure can be rolled back synchronously.
 */
struct process *process_create(const struct page_table *kernel_table)
{
    struct process *process = kmalloc(sizeof(*process));

    if (!process)
        return 0;
    if (!address_space_init(&process->as, kernel_table)) {
        kfree(process);
        return 0;
    }
    return process;
}

void process_destroy(struct process *process)
{
    /*
     * The current console descriptors are borrowed static objects.  Clearing
     * them documents that no file reference is released here yet.  Once VFS
     * files are refcounted, this loop becomes file_put() calls.
     */
    address_space_destroy(&process->as);
    for (size_t i = 0; i < 3; ++i)
        process->fds[i] = 0;
    kfree(process);
}

void process_reap(struct process *process)
{
    struct process_result *result = process->result;

    /*
     * Reaping runs from another thread after the dead thread has switched to
     * a different stack and page table.  Copy observable state first, destroy
     * the process, then set reaped last so a waiter never sees partial data.
     */
    if (result) {
        result->status = process->exit_status;
        result->fault = process->fault_code;
        result->user_preemptions = process->user_preemptions;
    }
    process_destroy(process);
    if (result)
        result->reaped = true;
}

_Noreturn void process_exit(int64_t status, uint64_t fault)
{
    struct process *process = current_process();

    /*
     * The exiting thread cannot free its own kernel stack or active page
     * table.  It only records the reason and becomes dead; scheduler_reap()
     * performs destruction later from a safe execution context.
     */
    process->exit_status = status;
    process->fault_code = fault;
    thread_exit();
}
