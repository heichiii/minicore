#include "trap.h"

#include <stddef.h>

#include "csr.h"
#include "sbi.h"
#include "../../platform/platform.h"

#define EXCEPTION_ILLEGAL_INSTRUCTION UINT64_C(2)
#define EXCEPTION_BREAKPOINT UINT64_C(3)
#define EXCEPTION_LOAD_ACCESS_FAULT UINT64_C(5)
#define EXCEPTION_LOAD_PAGE_FAULT UINT64_C(13)
#define EXCEPTION_STORE_PAGE_FAULT UINT64_C(15)
#define INTERRUPT_SUPERVISOR_TIMER UINT64_C(5)

_Static_assert(sizeof(struct trap_frame) == 36 * sizeof(uint64_t),
               "trap frame must match trap.S");
_Static_assert(offsetof(struct trap_frame, sepc) == 32 * sizeof(uint64_t),
               "sepc offset must match trap.S");
_Static_assert(offsetof(struct trap_frame, stval) == 35 * sizeof(uint64_t),
               "stval offset must match trap.S");

extern void trap_entry(void);
extern void trap_trigger_test_exceptions(void);
extern void trap_register_test(void);
extern char trap_register_window_start[], trap_register_window_end[];

uint64_t trap_register_test_sp;

static const uint64_t expected_exceptions[] = {
    EXCEPTION_ILLEGAL_INSTRUCTION,
    EXCEPTION_BREAKPOINT,
    EXCEPTION_LOAD_ACCESS_FAULT,
};

static volatile uint64_t timer_interval;
static volatile uint32_t timer_ticks;
static volatile uint32_t register_ticks;
static volatile uint32_t register_failures;
static size_t exception_index;
static bool test_running;
static volatile bool page_fault_test_running;
static volatile bool page_fault_seen;
static volatile uint64_t expected_page_fault;
static volatile uintptr_t expected_fault_address;

static void print_trap(const char *kind, const struct trap_frame *frame)
{
    console_puts(kind);
    console_puts(": scause=");
    console_puthex(frame->scause);
    console_puts(" sepc=");
    console_puthex(frame->sepc);
    console_puts(" stval=");
    console_puthex(frame->stval);
    console_putc('\n');
}

static const char *exception_name(uint64_t code)
{
    switch (code) {
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        return "illegal instruction";
    case EXCEPTION_BREAKPOINT:
        return "breakpoint";
    case EXCEPTION_LOAD_ACCESS_FAULT:
        return "load access fault";
    default:
        return "unexpected exception";
    }
}

static _Noreturn void unexpected(const char *kind,
                                 const struct trap_frame *frame)
{
    print_trap(kind, frame);
    platform_exit(false);
}

static size_t trapped_instruction_size(uint64_t sepc)
{
    const volatile uint16_t *instruction = (const volatile uint16_t *)sepc;

    return ((*instruction & 3U) == 3U) ? 4U : 2U;
}

static bool in_register_window(uint64_t sepc)
{
    return sepc >= (uintptr_t)trap_register_window_start &&
           sepc < (uintptr_t)trap_register_window_end;
}

static void check_saved_registers(struct trap_frame *frame)
{
    for (size_t index = 1; index < 32; ++index) {
        uint64_t expected = index == 2 ? trap_register_test_sp : index;

        if (frame->x[index] != expected)
            ++register_failures;
    }
}

static void handle_timer(struct trap_frame *frame)
{
    ++timer_ticks;
    if (test_running && in_register_window(frame->sepc)) {
        check_saved_registers(frame);
        ++register_ticks;
        if (register_ticks == 3)
            frame->sepc = (uintptr_t)trap_register_window_end;
    }
    sbi_set_timer(csr_read_time() + timer_interval);
}

void trap_dispatch(struct trap_frame *frame)
{
    uint64_t code = frame->scause & ~SCAUSE_INTERRUPT;

    if ((frame->scause & SCAUSE_INTERRUPT) != 0) {
        if (code == INTERRUPT_SUPERVISOR_TIMER) {
            handle_timer(frame);
            return;
        }
        unexpected("unexpected interrupt", frame);
    }

    if (page_fault_test_running && code == expected_page_fault &&
        frame->stval == expected_fault_address) {
        print_trap(code == EXCEPTION_LOAD_PAGE_FAULT ? "load page fault"
                                                     : "store page fault",
                   frame);
        page_fault_seen = true;
        frame->sepc += trapped_instruction_size(frame->sepc);
        return;
    }

    print_trap(exception_name(code), frame);
    if (test_running && exception_index <
            sizeof(expected_exceptions) / sizeof(expected_exceptions[0]) &&
        code == expected_exceptions[exception_index]) {
        ++exception_index;
        frame->sepc += trapped_instruction_size(frame->sepc);
        return;
    }

    unexpected("unhandled exception", frame);
}

bool trap_expect_page_fault(volatile uint64_t *address, bool write)
{
    uint64_t ignored = 0;

    expected_page_fault = write ? EXCEPTION_STORE_PAGE_FAULT
                                : EXCEPTION_LOAD_PAGE_FAULT;
    expected_fault_address = (uintptr_t)address;
    page_fault_seen = false;
    page_fault_test_running = true;
    if (write)
        *address = UINT64_C(0xfeedfacecafebeef);
    else
        ignored = *address;
    page_fault_test_running = false;
    __asm__ volatile("" : : "r"(ignored) : "memory");
    return page_fault_seen;
}

void trap_init(void)
{
    csr_clear_sstatus(SSTATUS_SIE);
    csr_clear_sie(UINT64_MAX);
    csr_write_stvec((uintptr_t)trap_entry);
}

bool trap_run_m1_tests(uint64_t timebase_frequency)
{
    exception_index = 0;
    timer_ticks = 0;
    register_ticks = 0;
    register_failures = 0;
    test_running = true;

    trap_trigger_test_exceptions();
    if (exception_index !=
        sizeof(expected_exceptions) / sizeof(expected_exceptions[0]))
        return false;

    timer_interval = timebase_frequency / 100;
    if (timer_interval == 0)
        timer_interval = 1;
    sbi_set_timer(csr_read_time() + timer_interval);
    csr_set_sie(SIE_STIE);
    csr_set_sstatus(SSTATUS_SIE);
    trap_register_test();
    csr_clear_sstatus(SSTATUS_SIE);
    csr_clear_sie(SIE_STIE);
    sbi_set_timer(UINT64_MAX);
    test_running = false;

    console_puts("timer ticks = ");
    console_puthex(timer_ticks);
    console_puts(" register checks = ");
    console_puthex(register_ticks);
    console_putc('\n');
    return register_ticks >= 3 && register_failures == 0;
}
