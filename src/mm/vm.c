#include "vm.h"

#include "layout.h"
#include "page_alloc.h"
#include "../runtime.h"

#define PTE_PPN_MASK UINT64_C(0x003ffffffffffc00)
#define SATP_SV39 (UINT64_C(8) << 60)

extern char __text_start[], __text_end[], __rodata_start[], __rodata_end[];
extern char __data_start[], __kernel_end[], __boot_stack_guard[];

static bool canonical(uint64_t address)
{
    uint64_t upper = address >> 39;
    return upper == 0 || upper == UINT64_C(0x1ffffff);
}

static uint64_t pte_physical(uint64_t pte)
{
    return (pte & PTE_PPN_MASK) << 2;
}

static uint64_t make_table_pte(uint64_t physical)
{
    return (physical >> 2) | PTE_V;
}

static uint64_t *root_pointer(const struct page_table *table)
{
    return phys_to_virt(table->root);
}

static size_t vpn_index(uint64_t address, unsigned level)
{
    return (address >> (12 + level * 9)) & 0x1ffU;
}

static bool valid_leaf_flags(uint64_t flags)
{
    const uint64_t allowed = PTE_R | PTE_W | PTE_X | PTE_U | PTE_G;

    return (flags & ~allowed) == 0 && (flags & (PTE_R | PTE_X)) != 0 &&
           !((flags & PTE_W) != 0 && (flags & PTE_R) == 0);
}

static uint64_t leaf_pte(uint64_t physical, uint64_t flags)
{
    uint64_t hardware = flags | PTE_V | PTE_A;

    if ((flags & PTE_W) != 0)
        hardware |= PTE_D;
    return (physical >> 2) | hardware;
}

bool vm_create(struct page_table *table)
{
    if (!table)
        return false;
    table->root = page_alloc();
    if (!table->root)
        return false;
    memset(phys_to_virt(table->root), 0, PAGE_SIZE);
    return true;
}

static uint64_t *walk(struct page_table *table, uint64_t address, bool create)
{
    uint64_t *entries;

    if (!table || !table->root || !canonical(address))
        return 0;
    entries = root_pointer(table);
    for (unsigned level = 2; level > 0; --level) {
        uint64_t *entry = &entries[vpn_index(address, level)];

        if ((*entry & PTE_V) == 0) {
            uint64_t page;

            if (!create)
                return 0;
            page = page_alloc();
            if (!page)
                return 0;
            memset(phys_to_virt(page), 0, PAGE_SIZE);
            *entry = make_table_pte(page);
        } else if ((*entry & (PTE_R | PTE_W | PTE_X)) != 0) {
            return 0;
        }
        entries = phys_to_virt(pte_physical(*entry));
    }
    return &entries[vpn_index(address, 0)];
}

bool vm_map(struct page_table *table, uint64_t virtual_address,
            uint64_t physical_address, uint64_t flags)
{
    uint64_t *entry;

    if ((virtual_address & PAGE_MASK) != 0 ||
        (physical_address & PAGE_MASK) != 0 || !valid_leaf_flags(flags))
        return false;
    entry = walk(table, virtual_address, true);
    if (!entry || (*entry & PTE_V) != 0)
        return false;
    *entry = leaf_pte(physical_address, flags);
    return true;
}

static bool table_empty(const uint64_t *entries)
{
    for (size_t i = 0; i < 512; ++i) {
        if ((entries[i] & PTE_V) != 0)
            return false;
    }
    return true;
}

bool vm_unmap(struct page_table *table, uint64_t address)
{
    uint64_t *levels[3];
    size_t indices[3];

    if (!table || !table->root || (address & PAGE_MASK) != 0 ||
        !canonical(address))
        return false;
    levels[2] = root_pointer(table);
    for (unsigned level = 2; level > 0; --level) {
        uint64_t entry;

        indices[level] = vpn_index(address, level);
        entry = levels[level][indices[level]];
        if ((entry & PTE_V) == 0 ||
            (entry & (PTE_R | PTE_W | PTE_X)) != 0)
            return false;
        levels[level - 1] = phys_to_virt(pte_physical(entry));
    }
    indices[0] = vpn_index(address, 0);
    if ((levels[0][indices[0]] & (PTE_V | PTE_R | PTE_X)) == PTE_V)
        return false;
    if ((levels[0][indices[0]] & PTE_V) == 0)
        return false;
    levels[0][indices[0]] = 0;
    for (unsigned level = 0; level < 2; ++level) {
        if (!table_empty(levels[level]))
            break;
        page_free(virt_to_phys(levels[level]));
        levels[level + 1][indices[level + 1]] = 0;
    }
    vm_flush_all();
    return true;
}

bool vm_protect(struct page_table *table, uint64_t address, uint64_t flags)
{
    uint64_t *entry;

    if ((address & PAGE_MASK) != 0 || !valid_leaf_flags(flags))
        return false;
    entry = walk(table, address, false);
    if (!entry || (*entry & PTE_V) == 0 ||
        (*entry & (PTE_R | PTE_X)) == 0)
        return false;
    *entry = leaf_pte(pte_physical(*entry), flags);
    vm_flush_all();
    return true;
}

bool vm_query(const struct page_table *table, uint64_t address,
              uint64_t *physical, uint64_t *flags)
{
    uint64_t *entry = walk((struct page_table *)table, address, false);

    if (!entry || (*entry & PTE_V) == 0 ||
        (*entry & (PTE_R | PTE_X)) == 0)
        return false;
    if (physical)
        *physical = pte_physical(*entry) | (address & PAGE_MASK);
    if (flags)
        *flags = *entry & 0x3ffU;
    return true;
}

static void destroy_level(uint64_t physical, unsigned level)
{
    uint64_t *entries = phys_to_virt(physical);

    if (level != 0) {
        for (size_t i = 0; i < 512; ++i) {
            uint64_t entry = entries[i];

            if ((entry & PTE_V) != 0 &&
                (entry & (PTE_R | PTE_W | PTE_X)) == 0)
                destroy_level(pte_physical(entry), level - 1);
        }
    }
    page_free(physical);
}

void vm_destroy(struct page_table *table)
{
    if (!table || !table->root)
        return;
    destroy_level(table->root, 2);
    table->root = 0;
}

void vm_flush_all(void)
{
    __asm__ volatile("sfence.vma zero, zero" : : : "memory");
}

void vm_activate(const struct page_table *table)
{
    uint64_t satp = SATP_SV39 | (table->root >> 12);

    __asm__ volatile("csrw satp, %0\nsfence.vma zero, zero"
                     : : "r"(satp) : "memory");
}

static uint64_t kernel_page_flags(uint64_t physical)
{
    uint64_t text_start = virt_to_phys(__text_start);
    uint64_t text_end = virt_to_phys(__text_end);
    uint64_t rodata_start = virt_to_phys(__rodata_start);
    uint64_t rodata_end = virt_to_phys(__rodata_end);

    if (physical >= (text_start & ~PAGE_MASK) &&
        physical < ((text_end + PAGE_MASK) & ~PAGE_MASK))
        return PTE_R | PTE_X | PTE_G;
    if (physical >= (rodata_start & ~PAGE_MASK) &&
        physical < ((rodata_end + PAGE_MASK) & ~PAGE_MASK))
        return PTE_R | PTE_G;
    return PTE_R | PTE_W | PTE_G;
}

static bool map_physical_range(struct page_table *table, uint64_t base,
                               uint64_t size, bool ram)
{
    uint64_t first = base & ~PAGE_MASK;
    uint64_t end;

    if (base > UINT64_MAX - size || base + size > UINT64_MAX - PAGE_MASK)
        return false;
    end = (base + size + PAGE_MASK) & ~PAGE_MASK;
    for (uint64_t physical = first; physical < end; physical += PAGE_SIZE) {
        uint64_t virtual_address = KERNEL_DIRECT_BASE + physical;
        uint64_t flags = ram ? kernel_page_flags(physical) : PTE_R | PTE_W | PTE_G;

        if (virtual_address == (uint64_t)(uintptr_t)__boot_stack_guard)
            continue;
        if (!vm_map(table, virtual_address, physical, flags))
            return false;
    }
    return true;
}

bool vm_build_kernel(struct page_table *table, const struct boot_info *info)
{
    if (!info || !vm_create(table))
        return false;
    for (size_t i = 0; i < info->memory_count; ++i) {
        if (!map_physical_range(table, info->memory[i].base,
                                info->memory[i].size, true))
            goto fail;
    }
    if (!map_physical_range(table, UINT64_C(0x100000), PAGE_SIZE, false) ||
        (info->uart_present &&
         !map_physical_range(table, info->uart.base, info->uart.size, false)) ||
        (info->plic_present &&
         !map_physical_range(table, info->plic.base, info->plic.size, false)))
        goto fail;
    for (size_t i = 0; i < info->virtio_count; ++i) {
        if (!map_physical_range(table, info->virtio[i].base,
                                info->virtio[i].size, false))
            goto fail;
    }
    return true;
fail:
    vm_destroy(table);
    return false;
}
