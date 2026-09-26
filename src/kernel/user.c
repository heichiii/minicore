#include "user.h"

#include "../arch/riscv/csr.h"
#include "../mm/layout.h"
#include "../mm/page_alloc.h"
#include "../platform/platform.h"
#include "../runtime.h"
#include "scheduler.h"

#define USER_CODE UINT64_C(0x10000)
#define USER_STACK_TOP UINT64_C(0x400000)
#define USER_STACK_PAGES 2U
#define SYSCALL_WRITE UINT64_C(1)
#define SYSCALL_EXIT UINT64_C(2)
#define SYSCALL_YIELD UINT64_C(3)
#define EXCEPTION_USER_ECALL UINT64_C(8)
#define EXCEPTION_LOAD_PAGE_FAULT UINT64_C(13)
#define EXCEPTION_STORE_PAGE_FAULT UINT64_C(15)

struct file;

struct file_ops {
    int64_t (*write)(struct file *file, uint64_t buffer, size_t size);
};

struct file {
    const struct file_ops *ops;
};

struct user_process {
    struct page_table page_table;
    uint64_t *pages;
    size_t page_count;
    struct file *fds[3];
    int64_t exit_status;
    uint64_t fault_code;
    bool finished;
};

extern const unsigned char _binary_build_user_bin_start[];
extern const unsigned char _binary_build_user_bin_end[];
extern void user_enter(uint64_t entry, uint64_t stack, uint64_t argument);
extern void user_resume_kernel(void);

uint64_t user_kernel_sp;
static struct user_process *current;

static bool copy_user(void *kernel_buffer, uint64_t user_address, size_t size,
                      bool to_user)
{
    unsigned char *buffer = kernel_buffer;

    if (!current || user_address > UINT64_MAX - size)
        return false;
    while (size != 0) {
        uint64_t physical;
        uint64_t flags;
        size_t chunk = PAGE_SIZE - (user_address & PAGE_MASK);

        if (chunk > size)
            chunk = size;
        if (!vm_query(&current->page_table, user_address, &physical, &flags) ||
            (flags & (PTE_U | PTE_R)) != (PTE_U | PTE_R) ||
            (to_user && (flags & PTE_W) == 0))
            return false;
        if (to_user)
            memcpy(phys_to_virt(physical), buffer, chunk);
        else
            memcpy(buffer, phys_to_virt(physical), chunk);
        buffer += chunk;
        user_address += chunk;
        size -= chunk;
    }
    return true;
}

bool copy_from_user(void *destination, uint64_t source, size_t size)
{
    return size == 0 ||
           (destination && copy_user(destination, source, size, false));
}

bool copy_to_user(uint64_t destination, const void *source, size_t size)
{
    return size == 0 ||
           (source && copy_user((void *)source, destination, size, true));
}

static int64_t console_write(struct file *file, uint64_t address, size_t size)
{
    char buffer[64];
    size_t done = 0;

    (void)file;
    if (address > UINT64_MAX - size)
        return -USER_EFAULT;
    while (done < size) {
        size_t chunk = size - done;

        if (chunk > sizeof(buffer))
            chunk = sizeof(buffer);
        if (!copy_from_user(buffer, address + done, chunk))
            return -USER_EFAULT;
        for (size_t i = 0; i < chunk; ++i)
            console_putc(buffer[i]);
        done += chunk;
    }
    return (int64_t)done;
}

static const struct file_ops console_ops = { .write = console_write };
static struct file console_file = { .ops = &console_ops };

static void finish_user(struct trap_frame *frame, int64_t status,
                        uint64_t fault)
{
    current->finished = true;
    current->exit_status = status;
    current->fault_code = fault;
    frame->x[1] = (uintptr_t)user_resume_kernel;
    frame->x[2] = user_kernel_sp;
    frame->sepc = (uintptr_t)user_resume_kernel;
    /* Return to the scheduler's supervisor context with interrupts enabled.
     * sret copies SPIE into SIE, so setting SPIE here is intentional. */
    frame->sstatus |= SSTATUS_SPP | SSTATUS_SPIE;
    frame->sstatus &= ~SSTATUS_SIE;
}

static int64_t dispatch_syscall(struct trap_frame *frame)
{
    switch (frame->x[17]) {
    case SYSCALL_WRITE: {
        uint64_t fd = frame->x[10];
        struct file *file;

        if (fd >= sizeof(current->fds) / sizeof(current->fds[0]) ||
            !(file = current->fds[fd]) || !file->ops || !file->ops->write)
            return -9;
        return file->ops->write(file, frame->x[11], (size_t)frame->x[12]);
    }
    case SYSCALL_EXIT:
        finish_user(frame, (int64_t)frame->x[10], 0);
        return 0;
    case SYSCALL_YIELD:
        scheduler_yield();
        return 0;
    default:
        return -38;
    }
}

bool user_handle_trap(struct trap_frame *frame, uint64_t code)
{
    if (!current || (frame->sstatus & SSTATUS_SPP) != 0)
        return false;
    if (code == EXCEPTION_USER_ECALL) {
        frame->sepc += 4;
        frame->x[10] = (uint64_t)dispatch_syscall(frame);
        return true;
    }
    finish_user(frame, -1, code);
    return true;
}

static void release_process(struct user_process *process)
{
    for (size_t i = 0; i < process->page_count; ++i) {
        if (process->pages[i])
            page_free(process->pages[i]);
    }
    vm_destroy_user(&process->page_table);
}

static bool prepare_process(struct user_process *process,
                            const struct page_table *kernel_table,
                            uint64_t *pages, size_t page_capacity)
{
    size_t image_size = (size_t)(_binary_build_user_bin_end -
                                 _binary_build_user_bin_start);
    size_t code_pages = (image_size + PAGE_MASK) / PAGE_SIZE;

    memset(process, 0, sizeof(*process));
    process->pages = pages;
    if (code_pages + USER_STACK_PAGES > page_capacity ||
        !vm_create_user(&process->page_table, kernel_table))
        return false;
    for (size_t i = 0; i < code_pages; ++i) {
        size_t offset = i * PAGE_SIZE;
        size_t chunk = image_size - offset;
        uint64_t page = page_alloc();

        if (chunk > PAGE_SIZE)
            chunk = PAGE_SIZE;
        if (!page)
            goto fail;
        pages[process->page_count++] = page;
        memset(phys_to_virt(page), 0, PAGE_SIZE);
        memcpy(phys_to_virt(page), _binary_build_user_bin_start + offset,
               chunk);
        if (!vm_map(&process->page_table, USER_CODE + offset, page,
                    PTE_U | PTE_R | PTE_X))
            goto fail;
    }
    for (size_t i = 0; i < USER_STACK_PAGES; ++i) {
        uint64_t page = page_alloc();
        uint64_t address = USER_STACK_TOP -
                           (USER_STACK_PAGES - i) * PAGE_SIZE;

        if (!page)
            goto fail;
        pages[process->page_count++] = page;
        memset(phys_to_virt(page), 0, PAGE_SIZE);
        if (!vm_map(&process->page_table, address, page,
                    PTE_U | PTE_R | PTE_W))
            goto fail;
    }
    process->fds[1] = &console_file;
    process->fds[2] = &console_file;
    return true;
fail:
    release_process(process);
    return false;
}

static bool run_case(const struct page_table *kernel_table, uint64_t mode,
                     int64_t expected_status, uint64_t expected_fault)
{
    struct user_process process;
    uint64_t pages[18] = {0};

    if (!prepare_process(&process, kernel_table, pages,
                         sizeof(pages) / sizeof(pages[0])))
        return false;
    current = &process;
    vm_activate(&process.page_table);
    user_enter(USER_CODE, USER_STACK_TOP - 16, mode);
    vm_activate(kernel_table);
    current = 0;
    bool passed = process.finished && process.exit_status == expected_status &&
                  process.fault_code == expected_fault;
    release_process(&process);
    return passed;
}

static bool run_copy_test(const struct page_table *kernel_table)
{
    static const char source[] = "copytest";
    struct user_process process;
    uint64_t pages[18] = {0};
    char result[sizeof(source)];
    bool passed;

    if (!prepare_process(&process, kernel_table, pages,
                         sizeof(pages) / sizeof(pages[0])))
        return false;
    current = &process;
    passed = copy_to_user(UINT64_C(0x3feffc), source, sizeof(source)) &&
             copy_from_user(result, UINT64_C(0x3feffc), sizeof(result)) &&
             memcmp(source, result, sizeof(source)) == 0 &&
             !copy_to_user(USER_CODE, source, 1) &&
             !copy_from_user(result, UINT64_MAX - 2, sizeof(result));
    current = 0;
    release_process(&process);
    return passed;
}

bool user_run_m3_tests(const struct page_table *kernel_table)
{
    size_t baseline = page_free_count();

    /* SUM deliberately remains clear: copies walk and validate PTEs and then
     * use the kernel direct map.  A bad U pointer therefore cannot fault the
     * kernel, and the permission check is identical across page boundaries. */
    csr_clear_sstatus(SSTATUS_SUM);
    if (!run_copy_test(kernel_table) ||
        !run_case(kernel_table, 0, 0, 0) ||
        !run_case(kernel_table, 1, -1, EXCEPTION_LOAD_PAGE_FAULT) ||
        !run_case(kernel_table, 2, -1, EXCEPTION_STORE_PAGE_FAULT) ||
        page_free_count() != baseline)
        return false;
    return true;
}
