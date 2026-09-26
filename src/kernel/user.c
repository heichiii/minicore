#include "user.h"

#include "../arch/riscv/csr.h"
#include "../mm/layout.h"
#include "../mm/page_alloc.h"
#include "../platform/platform.h"
#include "../runtime.h"
#include "scheduler.h"
#include "process.h"
#include "sync.h"

/*
 * User/kernel boundary and temporary M5 test-image launcher.
 *
 * The permanent responsibilities in this file are syscall dispatch, user
 * exception handling and checked user-memory transfers.  prepare_process()
 * and the acceptance tests below are scaffolding for the embedded flat image;
 * initramfs and the ELF loader will replace that loading path later in M5.
 *
 * There is intentionally no global "current user" here.  A user execution is
 * a real scheduler thread, and current_process() derives its process from the
 * scheduler's current thread.  This keeps the active process, kernel stack and
 * active page table synchronized at every context switch.
 */

/* Temporary flat-image layout; the ELF loader will choose segment addresses. */
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

/*
 * Minimal file abstraction introduced in M3 so write(2) does not call the
 * UART directly.  Full vnode/file definitions and reference counting arrive
 * with VFS; until then fd 1 and fd 2 borrow one static console_file.
 */
struct file_ops {
    int64_t (*write)(struct file *file, uint64_t buffer, size_t size);
};
struct file {
    const struct file_ops *ops;
};

extern const unsigned char _binary_build_user_bin_start[];
extern const unsigned char _binary_build_user_bin_end[];

bool copy_from_user(void *destination, uint64_t source, size_t size)
{
    /* The scheduler, rather than user.c, is the source of current identity. */
    struct process *process = current_process();
    return size == 0 || (process && address_space_copy(&process->as,
                            destination, source, size, false));
}

bool copy_to_user(uint64_t destination, const void *source, size_t size)
{
    struct process *process = current_process();
    return size == 0 || (process && address_space_copy(&process->as,
                            (void *)source, destination, size, true));
}

static int64_t console_write(struct file *file, uint64_t address, size_t size)
{
    char buffer[64];
    size_t done = 0;

    (void)file;
    /* Reject address + size overflow before advancing through the buffer. */
    if (address > UINT64_MAX - size)
        return -USER_EFAULT;

    /*
     * Copy through a small kernel buffer.  The UART never consumes a user
     * pointer directly, and every chunk is checked against the current
     * process's page table by copy_from_user().
     */
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

static int64_t dispatch_syscall(struct trap_frame *frame)
{
    struct process *current = current_process();

    /* RV64 ABI: a7 is the syscall number, a0-a2 are arguments, a0 is result. */
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
        process_exit((int64_t)frame->x[10], 0);
    case SYSCALL_YIELD:
        scheduler_yield();
        return 0;
    default:
        return -38;
    }
}

bool user_handle_trap(struct trap_frame *frame, uint64_t code)
{
    /*
     * SPP describes the privilege level interrupted by this trap.  A trap is
     * a user event only when it interrupted U-mode and the scheduled thread
     * owns a process.  Other traps remain for the generic trap dispatcher.
     */
    if (!current_process() || (frame->sstatus & SSTATUS_SPP) != 0)
        return false;
    if (code == EXCEPTION_USER_ECALL) {
        /* ecall is always a 32-bit instruction; resume after it. */
        frame->sepc += 4;
        frame->x[10] = (uint64_t)dispatch_syscall(frame);
        return true;
    }

    /* A user fault terminates only this process, never the kernel. */
    process_exit(-1, code);
}

/*
 * Build a process around the linker-embedded flat test image.
 *
 * Code is mapped U+RX, the two-page stack is U+RW and therefore NX, and the
 * page below the stack remains unmapped as a guard.  process_create() owns the
 * partially built address space, so one process_destroy() unwinds every error.
 * The ELF/initramfs step replaces this function without changing scheduling,
 * syscall dispatch or address-space ownership.
 */
static struct process *prepare_process(const struct page_table *kernel_table)
{
    size_t image_size = (size_t)(_binary_build_user_bin_end -
                                _binary_build_user_bin_start);
    struct process *process = process_create(kernel_table);

    if (!process)
        return 0;
    for (size_t offset = 0; offset < image_size; offset += PAGE_SIZE) {
        size_t chunk = image_size - offset;
        if (chunk > PAGE_SIZE)
            chunk = PAGE_SIZE;
        if (!address_space_add_page(&process->as, USER_CODE + offset,
                PTE_U | PTE_R | PTE_X,
                _binary_build_user_bin_start + offset, chunk))
            goto fail;
    }
    for (size_t i = 0; i < USER_STACK_PAGES; ++i) {
        if (!address_space_add_page(&process->as,
                USER_STACK_TOP - (USER_STACK_PAGES - i) * PAGE_SIZE,
                PTE_U | PTE_R | PTE_W, 0, 0))
            goto fail;
    }
    process->fds[1] = &console_file;
    process->fds[2] = &console_file;
    /*
     * The image was written through the kernel direct map.  fence.i makes
     * those stores visible to subsequent instruction fetches on this hart.
     */
    __asm__ volatile("fence.i" : : : "memory");
    return process;
fail:
    process_destroy(process);
    return 0;
}

static bool start_case(const struct page_table *kernel_table, uint64_t mode,
                       struct process_result *result)
{
    struct process *process = prepare_process(kernel_table);
    struct trap_frame frame = {0};

    memset(result, 0, sizeof(*result));
    if (!process)
        return false;
    /* result is borrowed from the supervising kernel test until reaping. */
    process->result = result;

    /*
     * Construct the register image consumed by user_thread_enter.  x2 is the
     * ABI stack pointer, a0 carries this test image's mode, and sepc is the
     * first user instruction.  SPP stays clear for U-mode; SPIE enables user
     * interrupts after sret.  The scheduler sanitizes sstatus once more when
     * it installs the frame on the new thread's private kernel stack.
     */
    frame.sepc = USER_CODE;
    frame.x[2] = USER_STACK_TOP - 16;
    frame.x[10] = mode;
    frame.sstatus = SSTATUS_SPIE; /* SPP, SUM and floating-point state clear. */
    /* Ownership passes to the scheduler only if publication succeeds. */
    if (!thread_create_user(process, &frame)) {
        process_destroy(process);
        return false;
    }
    return true;
}

static bool wait_result(struct process_result *result,
                        const struct page_table *kernel_table)
{
    bool passed = true;

    /*
     * This polling loop is test scaffolding, not waitpid.  Reap dead threads,
     * verify that the supervising kernel thread retained its execution
     * invariants, then yield until process_reap() publishes completion.
     */
    for (;;) {
        uint64_t flags;
        bool done;

        scheduler_reap();
        flags = irq_save();
        uint64_t scratch;
        __asm__ volatile("csrr %0, sscratch" : "=r"(scratch));
        if (current_process() || scratch != 0 ||
            (csr_read_sstatus() & SSTATUS_SUM) ||
            csr_read_satp() != ((UINT64_C(8) << 60) |
                                (kernel_table->root >> 12)))
            passed = false;
        done = result->reaped;
        irq_restore(flags);
        if (done)
            return passed;
        scheduler_yield();
    }
}

static bool run_case(const struct page_table *kernel_table, uint64_t mode,
                     int64_t status, uint64_t fault)
{
    struct process_result result;
    if (!start_case(kernel_table, mode, &result))
        return false;
    bool passed = wait_result(&result, kernel_table);
    return passed && result.status == status && result.fault == fault;
}

static bool run_copy_test(const struct page_table *kernel_table)
{
    static const char source[] = "copytest";
    struct process *process = prepare_process(kernel_table);
    char result[sizeof(source)];
    bool passed;

    if (!process)
        return false;
    /*
     * Exercise a cross-page transfer, reject writing RX text, reject an
     * overflowing range, reject the non-U kernel half and reject the unmapped
     * stack guard.  This test has no running thread, so it calls the lower
     * address-space primitive directly instead of copy_{from,to}_user().
     */
    passed = address_space_copy(&process->as, (void *)source,
                                UINT64_C(0x3feffc), sizeof(source), true) &&
             address_space_copy(&process->as, result, UINT64_C(0x3feffc),
                                sizeof(result), false) &&
             memcmp(source, result, sizeof(result)) == 0 &&
             !address_space_copy(&process->as, (void *)source, USER_CODE,
                                 1, true) &&
             !address_space_copy(&process->as, result, UINT64_MAX - 2,
                                 sizeof(result), false) &&
             !address_space_copy(&process->as, result,
                                 UINT64_C(0xffffffc080200000), 1, false) &&
             !address_space_copy(&process->as, result,
                                 USER_STACK_TOP - 3 * PAGE_SIZE, 1, false);
    process_destroy(process);
    return passed;
}

bool user_run_m3_tests(const struct page_table *kernel_table)
{
    size_t baseline = page_free_count();

    /* Checked copies use the direct map, so supervisor access to U pages stays off. */
    csr_clear_sstatus(SSTATUS_SUM);
    return run_copy_test(kernel_table) &&
           run_case(kernel_table, 0, 0, 0) &&
           run_case(kernel_table, 1, -1, EXCEPTION_LOAD_PAGE_FAULT) &&
           run_case(kernel_table, 2, -1, EXCEPTION_STORE_PAGE_FAULT) &&
           page_free_count() == baseline;
}

/*
 * Hold every free page in an intrusive physical-page list, then return one
 * page per iteration.  This walks through low-memory budgets that fail at
 * different points in process, page-table, user-page-record and thread-stack
 * creation.  Each attempt must restore the exact free-page count it started
 * with, whether publication succeeds or construction rolls back.
 */
static bool run_allocation_tests(const struct page_table *kernel_table)
{
    size_t baseline = page_free_count();
    uint64_t held = 0, page;
    bool passed = true;
    bool saw_failure = false, saw_success = false;

    while ((page = page_alloc()) != 0) {
        *(uint64_t *)phys_to_virt(page) = held;
        held = page;
    }
    for (unsigned budget = 0; budget <= 24; ++budget) {
        size_t before = page_free_count();
        struct process_result result;
        if (start_case(kernel_table, 0, &result)) {
            saw_success = true;
            bool waited = wait_result(&result, kernel_table);
            passed = waited && passed && result.status == 0 && result.fault == 0;
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

bool user_run_thread_tests(const struct page_table *kernel_table)
{
    size_t baseline = page_free_count();

    if (!run_allocation_tests(kernel_table))
        return false;
    console_puts("M5 ALLOCATION ROLLBACK PASS\n");
    for (unsigned round = 0; round < 3; ++round) {
        struct process_result results[3];
        unsigned started = 0;
        bool passed = true;

        /*
         * Publish two surviving users and one faulting user while interrupts
         * are disabled.  This guarantees all three exist before timer
         * preemption can select one, so the test exercises real coexistence.
         */
        uint64_t flags = irq_save();
        for (; started < 3; ++started) {
            if (!start_case(kernel_table, started + 3, &results[started])) {
                passed = false;
                break;
            }
        }
        irq_restore(flags);
        for (unsigned i = 0; i < started; ++i) {
            bool waited = wait_result(&results[i], kernel_table);
            passed = waited && passed;
        }
        if (!passed)
            return false;
        /* The two survivors must exit normally after repeated U-mode ticks. */
        for (unsigned i = 0; i < 2; ++i) {
            if (results[i].status != 0 || results[i].fault != 0 ||
                results[i].user_preemptions < 2)
                return false;
        }
        /*
         * The illegal-instruction process must die alone.  Once all three are
         * reaped, execution must be back in the kernel address space and every
         * user allocation must have returned to the baseline.
         */
        if (results[2].status != -1 || results[2].fault != 2 ||
            current_process() != 0 ||
            csr_read_satp() != ((UINT64_C(8) << 60) |
                                (kernel_table->root >> 12)) ||
            page_free_count() != baseline)
            return false;
    }
    return true;
}
