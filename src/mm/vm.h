#ifndef MINICORE_MM_VM_H
#define MINICORE_MM_VM_H

#include <stdbool.h>
#include <stdint.h>

#include "../platform/dtb.h"

#define PTE_V (UINT64_C(1) << 0)
#define PTE_R (UINT64_C(1) << 1)
#define PTE_W (UINT64_C(1) << 2)
#define PTE_X (UINT64_C(1) << 3)
#define PTE_U (UINT64_C(1) << 4)
#define PTE_G (UINT64_C(1) << 5)
#define PTE_A (UINT64_C(1) << 6)
#define PTE_D (UINT64_C(1) << 7)

struct page_table {
    uint64_t root;
};

bool vm_create(struct page_table *table);
void vm_destroy(struct page_table *table);
bool vm_map(struct page_table *table, uint64_t virtual_address,
            uint64_t physical_address, uint64_t flags);
bool vm_unmap(struct page_table *table, uint64_t virtual_address);
bool vm_protect(struct page_table *table, uint64_t virtual_address,
                uint64_t flags);
bool vm_query(const struct page_table *table, uint64_t virtual_address,
              uint64_t *physical_address, uint64_t *flags);
void vm_activate(const struct page_table *table);
void vm_flush_all(void);
bool vm_build_kernel(struct page_table *table, const struct boot_info *info);

#endif
