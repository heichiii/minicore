#ifndef MINICORE_KERNEL_USER_H
#define MINICORE_KERNEL_USER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../arch/riscv/trap.h"
#include "../mm/vm.h"

#define USER_EFAULT 14

bool user_run_m3_tests(const struct page_table *kernel_table);
bool user_handle_trap(struct trap_frame *frame, uint64_t code);
bool copy_from_user(void *destination, uint64_t source, size_t size);
bool copy_to_user(uint64_t destination, const void *source, size_t size);

#endif
