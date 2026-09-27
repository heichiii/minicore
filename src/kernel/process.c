#include "process.h"

#include "elf.h"
#include "scheduler.h"
#include "user.h"
#include "../arch/riscv/csr.h"
#include "../fs/vfs.h"
#include "../mm/heap.h"
#include "../runtime.h"

#define PROCESS_ECHILD 10
#define PROCESS_EFAULT 14
#define PROCESS_EINVAL 22
#define PROCESS_ENOMEM 12

static const struct page_table *kernel_page_table;
static struct process *init_process;
static uint64_t next_pid;
static struct spinlock process_lock;

/* All family lists, exit-state transitions, and wait queues share this lock. */
void process_system_init(const struct page_table *kernel_table)
{
    kernel_page_table = kernel_table;
    init_process = 0;
    next_pid = 1;
    spinlock_init(&process_lock);
}

struct process *process_create(void)
{
    struct process *process = kmalloc(sizeof(*process));
    uint64_t flags;

    if (!process)
        return 0;
    if (!address_space_init(&process->as, kernel_page_table)) {
        kfree(process);
        return 0;
    }
    list_init(&process->child_link);
    list_init(&process->children);
    wait_queue_init(&process->child_wait);
    process->state = PROCESS_RUNNING;
    flags = irq_save();
    spin_lock(&process_lock);
    process->pid = next_pid++;
    spin_unlock(&process_lock);
    irq_restore(flags);
    return process;
}

void process_destroy(struct process *process)
{
    if (!process)
        return;
    address_space_destroy(&process->as);
    for (size_t i = 0; i < 3; ++i) {
        file_put(process->fds[i]);
        process->fds[i] = 0;
    }
    kfree(process);
}

void process_set_init(struct process *process)
{
    init_process = process;
}

uint64_t process_getpid(void)
{
    struct process *process = current_process();

    return process ? process->pid : 0;
}

static void link_child(struct process *parent, struct process *child)
{
    child->parent = parent;
    list_push_back(&parent->children, &child->child_link);
}

int64_t process_fork(const struct trap_frame *parent_frame)
{
    struct process *parent = current_process();
    struct process *child = process_create();
    struct address_space copy;
    struct trap_frame child_frame;
    uint64_t flags;

    if (!child)
        return -PROCESS_ENOMEM;
    /*
     * Build every fallible child resource before publishing a runnable thread.
     * This is eager copying: the child's pages have distinct physical storage
     * immediately, while open file objects remain shared by reference.
     */
    address_space_destroy(&child->as);
    if (!address_space_clone(&copy, &parent->as, kernel_page_table)) {
        child->as.table.root = 0;
        list_init(&child->as.pages);
        process_destroy(child);
        return -PROCESS_ENOMEM;
    }
    address_space_move(&child->as, &copy);
    for (size_t i = 0; i < 3; ++i) {
        child->fds[i] = parent->fds[i];
        file_get(child->fds[i]);
    }
    child_frame = *parent_frame;
    child_frame.x[10] = 0;

    flags = irq_save();
    spin_lock(&process_lock);
    link_child(parent, child);
    spin_unlock(&process_lock);
    if (!thread_create_user(child, &child_frame)) {
        spin_lock(&process_lock);
        list_remove(&child->child_link);
        child->parent = 0;
        spin_unlock(&process_lock);
        irq_restore(flags);
        process_destroy(child);
        return -PROCESS_ENOMEM;
    }
    irq_restore(flags);
    return (int64_t)child->pid;
}

int64_t process_exec(struct trap_frame *frame, const char *path,
                     const struct exec_arguments *arguments)
{
    struct process *process = current_process();
    struct exec_image image;
    struct address_space old;

    if (!elf_load(kernel_page_table, path, arguments, &image))
        return -2;

    /*
     * Commit only after the complete replacement image is valid.  The current
     * trap frame lives on the kernel stack, which is in the shared upper half,
     * so activating the new page table before freeing the old one is safe.
     */
    address_space_move(&old, &process->as);
    address_space_move(&process->as, &image.as);
    vm_activate(&process->as.table);
    address_space_destroy(&old);

    memset(frame, 0, sizeof(*frame));
    frame->sepc = image.entry;
    frame->x[2] = image.stack_pointer;
    frame->sstatus = SSTATUS_SPIE;
    return 0;
}

static struct process *find_child(struct process *parent, int64_t pid)
{
    struct process *first = 0;

    for (struct list_node *entry = parent->children.next;
         entry != &parent->children; entry = entry->next) {
        struct process *child = container_of(entry, struct process, child_link);

        if (pid != -1 && (uint64_t)pid == child->pid)
            return child;
        if (pid == -1) {
            if (child->state == PROCESS_REAPABLE)
                return child;
            if (!first)
                first = child;
        }
    }
    return first;
}

int64_t process_waitpid(int64_t pid, uint64_t status_address,
                        uint64_t options)
{
    struct process *parent = current_process();
    uint64_t flags;

    if ((pid <= 0 && pid != -1) || options != 0)
        return -PROCESS_EINVAL;
    flags = irq_save();
    spin_lock(&process_lock);
    for (;;) {
        struct process *child = find_child(parent, pid);

        if (!child) {
            spin_unlock(&process_lock);
            irq_restore(flags);
            return -PROCESS_ECHILD;
        }
        if (child->state == PROCESS_REAPABLE) {
            int64_t status = child->exit_status;
            int64_t result = (int64_t)child->pid;

            if (status_address &&
                !copy_to_user(status_address, &status, sizeof(status))) {
                spin_unlock(&process_lock);
                irq_restore(flags);
                return -PROCESS_EFAULT;
            }
            list_remove(&child->child_link);
            child->parent = 0;
            spin_unlock(&process_lock);
            irq_restore(flags);
            kfree(child);
            return result;
        }
        /* Enqueue before releasing process_lock to avoid a lost exit wakeup. */
        wait_queue_sleep(&parent->child_wait, &process_lock);
    }
}

_Noreturn void process_exit(int64_t status, uint64_t fault)
{
    struct process *process = current_process();
    uint64_t flags = irq_save();

    spin_lock(&process_lock);
    process->exit_status = status;
    process->fault_code = fault;
    process->state = PROCESS_ZOMBIE;

    /* PID 1 adopts every live or zombie descendant of an exiting process. */
    while (!list_empty(&process->children)) {
        struct process *child = container_of(process->children.next,
                                              struct process, child_link);

        list_remove(&child->child_link);
        child->parent = 0;
        if (init_process && init_process != process)
            link_child(init_process, child);
        else if (child->state == PROCESS_REAPABLE)
            kfree(child);
    }
    spin_unlock(&process_lock);
    (void)flags;
    thread_exit();
}

void process_thread_reaped(struct process *process)
{
    struct process_result *result = process->result;

    /*
     * Runtime resources disappear before the small zombie record is exposed.
     * The scheduler calls here only after selecting another address space and
     * freeing the dead thread's kernel stack.
     */
    address_space_destroy(&process->as);
    for (size_t i = 0; i < 3; ++i) {
        file_put(process->fds[i]);
        process->fds[i] = 0;
    }

    spin_lock(&process_lock);
    process->state = PROCESS_REAPABLE;
    if (process->parent)
        wait_queue_wake_all(&process->parent->child_wait);
    if (!result) {
        spin_unlock(&process_lock);
        if (!process->parent)
            kfree(process);
        return;
    }
    result->status = process->exit_status;
    result->fault = process->fault_code;
    if (process == init_process)
        init_process = 0;
    spin_unlock(&process_lock);

    /* A root process has a kernel observer instead of a userspace parent. */
    kfree(process);
    result->reaped = true;
}
