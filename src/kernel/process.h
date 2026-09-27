#ifndef MINICORE_PROCESS_H
#define MINICORE_PROCESS_H

#include "list.h"
#include "sync.h"
#include "../arch/riscv/trap.h"
#include "../mm/address_space.h"
#include "../abi.h"

struct file;
struct vnode;
struct exec_arguments;

/* Kernel observer used only to wait for the root /init process. */
struct process_result {
    volatile bool reaped;
    int64_t status;
    uint64_t fault;
};

enum process_state {
    /* The owning thread may execute or sleep. */
    PROCESS_RUNNING,
    /* Exit was recorded, but its thread/VM may still be active. */
    PROCESS_ZOMBIE,
    /* Runtime resources are gone; waitpid may consume this record. */
    PROCESS_REAPABLE,
};

/*
 * A process owns resources shared across its execution: address space, open
 * files, identity, family links, and exit state.  Its one scheduler thread
 * separately owns the kernel stack and saved CPU context.  Keeping those
 * objects separate lets the scheduler leave a dead stack before this process's
 * runtime resources are released.
 */
struct process {
    struct address_space as;
    struct file *fds[FD_COUNT];
    /* Stable namespace pointers borrowed from mounts; inherited by fork and
     * preserved by exec. M6 permits chdir, but has no chroot or unmount. */
    struct vnode *root;
    struct vnode *cwd;
    uint64_t pid;
    struct process *parent;
    struct list_node child_link;
    struct list_node children;
    struct wait_queue child_wait;
    enum process_state state;
    int64_t exit_status;
    uint64_t fault_code;
    struct process_result *result;
};

void process_system_init(const struct page_table *kernel_table);
/* Create an unpublished process with an empty user address space and new PID. */
struct process *process_create(void);
/* Destroy an unpublished process; no thread or family link may refer to it. */
void process_destroy(struct process *process);
/* Register the root process as PID 1 and the orphan adopter. */
void process_set_init(struct process *process);
uint64_t process_getpid(void);
int64_t process_fork(const struct trap_frame *parent_frame);
int64_t process_exec(struct trap_frame *frame, const char *path,
                     const struct exec_arguments *arguments);
int64_t process_waitpid(int64_t pid, uint64_t status_address,
                        uint64_t options);
_Noreturn void process_exit(int64_t status, uint64_t fault);

/* Called by the scheduler only after it has left the dead thread's stack. */
void process_thread_reaped(struct process *process);

#endif
