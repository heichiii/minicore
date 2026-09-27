#include "user.h"

#include "elf.h"
#include "fd.h"
#include "process.h"
#include "scheduler.h"
#include "sync.h"
#include "../arch/riscv/csr.h"
#include "../fs/vfs.h"
#include "../mm/heap.h"
#include "../mm/layout.h"
#include "../mm/page_alloc.h"
#include "../platform/platform.h"
#include "../runtime.h"

#define EXCEPTION_USER_ECALL UINT64_C(8)

/* The linker creates these symbols from build/initramfs.cpio. */
extern const unsigned char _binary_build_initramfs_cpio_start[];
extern const unsigned char _binary_build_initramfs_cpio_end[];

bool copy_from_user(void *destination, uint64_t source, size_t size)
{
    struct process *process = current_process();

    return size == 0 ||
           (process && address_space_copy(&process->as, destination, source,
                                          size, false));
}

bool copy_to_user(uint64_t destination, const void *source, size_t size)
{
    struct process *process = current_process();

    return size == 0 ||
           (process && address_space_copy(&process->as, (void *)source,
                                          destination, size, true));
}

static bool copy_user_string(char *destination, size_t capacity,
                             uint64_t source)
{
    if (!source || !capacity)
        return false;
    for (size_t i = 0; i < capacity; ++i) {
        if (source > UINT64_MAX - i ||
            !copy_from_user(&destination[i], source + i, 1))
            return false;
        if (!destination[i])
            return true;
    }
    return false;
}

static bool copy_user_vector(uint64_t vector, char strings[][EXEC_STRING_SIZE],
                             size_t maximum, size_t *count)
{
    *count = 0;
    if (!vector)
        return true;
    if (vector > UINT64_MAX - (maximum + 1) * sizeof(uint64_t))
        return false;
    while (*count < maximum) {
        uint64_t pointer;

        if (!copy_from_user(&pointer, vector + *count * sizeof(pointer),
                            sizeof(pointer)))
            return false;
        if (!pointer)
            return true;
        if (!copy_user_string(strings[*count], EXEC_STRING_SIZE, pointer))
            return false;
        ++*count;
    }
    /* A bounded vector must still contain its terminating null pointer. */
    uint64_t terminator;
    return copy_from_user(&terminator, vector + maximum * sizeof(terminator),
                          sizeof(terminator)) && terminator == 0;
}

static bool copy_exec_request(uint64_t user_path, uint64_t user_argv,
                              uint64_t user_envp, char *path,
                              struct exec_arguments *arguments)
{
    /* Copy everything before exec can replace the source address space. */
    memset(arguments, 0, sizeof(*arguments));
    return copy_user_string(path, EXEC_STRING_SIZE, user_path) &&
           copy_user_vector(user_argv, arguments->argv, EXEC_MAX_ARGS,
                            &arguments->argc) &&
           copy_user_vector(user_envp, arguments->envp, EXEC_MAX_ENVS,
                            &arguments->envc);
}

/* Returns true when user_handle_trap should write result back to a0. */
static bool dispatch_syscall(struct trap_frame *frame, int64_t *result)
{
    struct process *current = current_process();

    switch (frame->x[17]) {
    case SYS_WRITE: case SYS_READ: case SYS_OPENAT: case SYS_CLOSE:
    case SYS_DUP: case SYS_DUP2: case SYS_GETDENTS: case SYS_PIPE:
    case SYS_CHDIR: case SYS_GETCWD: case SYS_LSEEK:
        *result = fd_syscall(frame);
        return true;
    case SYS_BRK:
        *result = address_space_brk(&current->as, frame->x[10]);
        return true;
    case SYS_MUNMAP:
        *result = address_space_munmap(&current->as, frame->x[10], frame->x[11]);
        return true;
    case SYS_MMAP: {
        uint64_t fd = frame->x[14];
        struct file *file = fd < FD_COUNT ? current->fds[fd] : 0;
        if (frame->x[12] > UINT32_MAX || frame->x[13] > UINT32_MAX ||
            ((frame->x[13] & MAP_ANONYMOUS) && (int64_t)fd != -1))
            *result = -E_INVAL;
        else
            *result = address_space_mmap(&current->as, frame->x[10],
                frame->x[11], (unsigned)frame->x[12], (unsigned)frame->x[13],
                file, frame->x[15]);
        return true;
    }
    case SYS_EXIT:
        process_exit((int64_t)frame->x[10], 0);
    case SYS_YIELD:
        scheduler_yield();
        *result = 0;
        return true;
    case SYS_GETPID:
        *result = (int64_t)process_getpid();
        return true;
    case SYS_EXEC: {
        struct exec_arguments *arguments = kmalloc(sizeof(*arguments));
        char path[EXEC_STRING_SIZE];

        if (!arguments) {
            *result = -12;
            return true;
        }
        if (!copy_exec_request(frame->x[10], frame->x[11], frame->x[12],
                               path, arguments)) {
            kfree(arguments);
            *result = -USER_EFAULT;
            return true;
        }
        *result = process_exec(frame, path, arguments);
        kfree(arguments);
        return *result != 0;
    }
    case SYS_WAITPID:
        *result = process_waitpid((int64_t)frame->x[10], frame->x[11],
                                  frame->x[12]);
        return true;
    case SYS_FORK:
        *result = process_fork(frame);
        return true;
    default:
        *result = -38;
        return true;
    }
}

bool user_handle_trap(struct trap_frame *frame, uint64_t code)
{
    int64_t result;

    if (!current_process() || (frame->sstatus & SSTATUS_SPP) != 0)
        return false;
    if (code == EXCEPTION_USER_ECALL) {
        frame->sepc += 4;
        if (dispatch_syscall(frame, &result))
            frame->x[10] = (uint64_t)result;
        return true;
    }
    process_exit(-1, code);
}

static bool start_init(const struct page_table *kernel_table,
                       struct process_result *result)
{
    struct exec_arguments arguments = {
        .argc = 1,
        .envc = 1,
        .argv = { "/init" },
        .envp = { "BOOT=init" },
    };
    struct exec_image image;
    struct process *process = process_create();
    struct trap_frame frame = {0};

    if (!process)
        return false;
    /* Replace process_create's empty address space with the prepared ELF. */
    address_space_destroy(&process->as);
    if (!elf_load(kernel_table, "/init", &arguments, &image)) {
        process->as.table.root = 0;
        list_init(&process->as.pages);
        process_destroy(process);
        return false;
    }
    address_space_move(&process->as, &image.as);
    if (fd_install_stdio(process)) {
        process_destroy(process);
        return false;
    }
    process->result = result;
    frame.sepc = image.entry;
    frame.x[2] = image.stack_pointer;
    frame.sstatus = SSTATUS_SPIE;
    /* Publish PID 1 before making its thread runnable. */
    process_set_init(process);
    if (!thread_create_user(process, &frame)) {
        process_set_init(0);
        process_destroy(process);
        return false;
    }
    return true;
}

static bool run_elf_allocation_test(const struct page_table *kernel_table)
{
    const struct exec_arguments arguments = {
        .argc = 1,
        .argv = { "/init" },
    };
    size_t baseline = page_free_count();
    uint64_t held = 0;
    uint64_t page;
    bool saw_failure = false;
    bool saw_success = false;
    bool passed = true;

    /* Store the temporary free-page list inside the held pages themselves. */
    while ((page = page_alloc()) != 0) {
        *(uint64_t *)phys_to_virt(page) = held;
        held = page;
    }
    for (unsigned budget = 0; budget <= 24; ++budget) {
        struct exec_image image;
        size_t before = page_free_count();

        if (elf_load(kernel_table, "/init", &arguments, &image)) {
            saw_success = true;
            elf_image_destroy(&image);
        } else {
            saw_failure = true;
        }
        if (page_free_count() != before) {
            passed = false;
            break;
        }
        if (held) {
            page = held;
            held = *(uint64_t *)phys_to_virt(page);
            page_free(page);
        }
    }
    while (held) {
        page = held;
        held = *(uint64_t *)phys_to_virt(page);
        page_free(page);
    }
    return passed && saw_failure && saw_success &&
           page_free_count() == baseline;
}

/* Exhaust real physical pages, then restore a small allocation budget. Every
 * operation must either finish or unwind to the same free-page count. This
 * catches intermediate page-table leaks that ordinary success tests miss. */
static bool run_m6_allocation_test(const struct page_table *kernel_table)
{
    size_t baseline = page_free_count();
    struct address_space source;
    if (!address_space_init(&source, kernel_table)) return false;
    int64_t mapping = address_space_mmap(&source, 0, 3 * PAGE_SIZE,
        PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
    if (mapping < 0) { address_space_destroy(&source); return false; }
    uint64_t held = 0, page;
    while ((page = page_alloc()) != 0) {
        *(uint64_t *)phys_to_virt(page) = held; held = page;
    }
    bool passed = true, clone_failed = false, clone_succeeded = false;
    bool map_failed = false, map_succeeded = false, brk_failed = false, brk_succeeded = false;
    for (unsigned budget = 0; budget <= 24; ++budget) {
        size_t before = page_free_count();
        struct address_space copy;
        if (address_space_clone(&copy, &source, kernel_table)) {
            clone_succeeded = true; address_space_destroy(&copy);
        } else clone_failed = true;
        if (page_free_count() != before) { passed = false; break; }
        int64_t at = address_space_mmap(&source, 0, 3 * PAGE_SIZE,
            PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, 0, 0);
        if (at >= 0) {
            map_succeeded = true;
            address_space_munmap(&source, (uint64_t)at, 3 * PAGE_SIZE);
        } else map_failed = true;
        if (page_free_count() != before) { passed = false; break; }
        if (address_space_brk(&source, source.brk_base + 3 * PAGE_SIZE) >= 0) {
            brk_succeeded = true;
            address_space_brk(&source, source.brk_base);
        } else brk_failed = true;
        struct file *reader, *writer;
        if (!pipe_create(&reader, &writer)) { file_put(reader); file_put(writer); }
        if (page_free_count() != before || source.brk_end != source.brk_base) {
            passed = false; break;
        }
        if (held) {
            page = held; held = *(uint64_t *)phys_to_virt(page); page_free(page);
        }
    }
    while (held) {
        page = held; held = *(uint64_t *)phys_to_virt(page); page_free(page);
    }
    address_space_destroy(&source);
    return passed && clone_failed && clone_succeeded && map_failed && map_succeeded &&
           brk_failed && brk_succeeded && page_free_count() == baseline;
}

bool user_run_tests(const struct page_table *kernel_table)
{
    struct process_result result = {0};
    size_t archive_size = (size_t)(_binary_build_initramfs_cpio_end -
                                   _binary_build_initramfs_cpio_start);
    size_t baseline = page_free_count();

    if (!vfs_init(_binary_build_initramfs_cpio_start, archive_size))
        return false;
    process_system_init(kernel_table);
    if (!run_elf_allocation_test(kernel_table))
        return false;
    console_puts("M5 ALLOCATION ROLLBACK PASS\n");
    if (!run_m6_allocation_test(kernel_table)) return false;
    console_puts("M6 ALLOCATION ROLLBACK PASS\n");
    if (!start_init(kernel_table, &result))
        return false;
    while (!result.reaped) {
        scheduler_reap();
        scheduler_yield();
    }
    /* Namespace storage lives longer than processes. Release it separately
     * so the final allocator check includes tmpfs pages and every vnode. */
    if (file_live_count()) return false;
    vfs_shutdown();
    if (result.status != 0 || result.fault != 0 ||
        page_free_count() != baseline) {
        console_puts("M5 init status=");
        console_puthex((uint64_t)result.status);
        console_puts(" fault=");
        console_puthex(result.fault);
        console_puts(" pages=");
        console_puthex(page_free_count());
        console_puts(" baseline=");
        console_puthex(baseline);
        console_putc('\n');
    }
    return result.status == 0 && result.fault == 0 &&
           current_process() == 0 && page_free_count() == baseline;
}
