#ifndef MINICORE_PROCESS_H
#define MINICORE_PROCESS_H

#include "../mm/address_space.h"

struct file;

/*
 * Temporary kernel-side completion object used by the M3/M5 acceptance
 * tests.  The test owns its storage and must keep it alive until reaped is
 * true.  The reaper writes all result fields before publishing reaped.
 *
 * This is deliberately not the userspace wait ABI.  PID, parent/child links,
 * zombie records and waitpid will replace it in the lifecycle step.
 */
struct process_result {
    bool reaped;
    int64_t status;
    uint64_t fault;
    uint64_t user_preemptions;
};

/*
 * Resources shared by the execution belonging to one process.  This stage
 * permits exactly one owning thread per process:
 *
 *   as               owns the user page table and user physical pages;
 *   fds              currently borrow static console objects;
 *   exit/fault       are filled exactly once when execution terminates;
 *   user_preemptions is acceptance-test instrumentation;
 *   result           is a borrowed observer used only by kernel tests.
 *
 * The scheduler thread owns its kernel stack and saved CPU state separately.
 * Later M5 work will add PID and parent/child state here; the VFS step will
 * replace borrowed console pointers with reference-counted open files.
 */
struct process {
    struct address_space as;
    struct file *fds[3];
    int64_t exit_status;
    uint64_t fault_code;
    uint64_t user_preemptions;
    struct process_result *result;
};

struct process *process_create(const struct page_table *kernel_table);

/* Destroy an unpublished process, or one whose thread has already stopped. */
void process_destroy(struct process *process);

/* Publish the final test result and destroy a dead process. */
void process_reap(struct process *process);

/* Record termination and switch away forever; this function cannot return. */
_Noreturn void process_exit(int64_t status, uint64_t fault);
#endif
