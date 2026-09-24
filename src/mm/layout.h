#ifndef MINICORE_MM_LAYOUT_H
#define MINICORE_MM_LAYOUT_H

#include <stdint.h>

#define PAGE_SIZE UINT64_C(4096)
#define PAGE_MASK (PAGE_SIZE - 1)
#define KERNEL_DIRECT_BASE UINT64_C(0xffffffc000000000)

static inline void *phys_to_virt(uint64_t physical)
{
    return (void *)(uintptr_t)(KERNEL_DIRECT_BASE + physical);
}

static inline uint64_t virt_to_phys(const void *virtual_address)
{
    return (uint64_t)(uintptr_t)virtual_address - KERNEL_DIRECT_BASE;
}

#endif
