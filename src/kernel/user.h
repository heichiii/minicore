#ifndef MINICORE_KERNEL_USER_H
#define MINICORE_KERNEL_USER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../arch/riscv/trap.h"
#include "../mm/vm.h"

#define USER_EFAULT 14

/* Kernel acceptance suites; these are not part of the user ABI. */
bool user_run_m3_tests(const struct page_table *kernel_table);
bool user_run_thread_tests(const struct page_table *kernel_table);

/*
 * Handle a synchronous trap whose saved privilege may be U-mode.  Returns
 * false when the trap did not belong to a current user process.  User exit and
 * fatal user exceptions do not return through this interface.
 */
bool user_handle_trap(struct trap_frame *frame, uint64_t code);

/*
 * Checked transfers relative to the current process's address space.  They
 * return false for an invalid range, missing mapping or insufficient PTE
 * permission rather than faulting the kernel.
 */
bool copy_from_user(void *destination, uint64_t source, size_t size);
bool copy_to_user(uint64_t destination, const void *source, size_t size);

#endif
