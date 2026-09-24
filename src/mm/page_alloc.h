#ifndef MINICORE_MM_PAGE_ALLOC_H
#define MINICORE_MM_PAGE_ALLOC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../platform/dtb.h"

bool page_allocator_init(const struct boot_info *info, uint64_t kernel_start,
                         uint64_t kernel_end, uint64_t dtb_start,
                         uint64_t dtb_size);
uint64_t page_alloc(void);
void page_free(uint64_t physical);
size_t page_free_count(void);
size_t page_total_count(void);

#endif
