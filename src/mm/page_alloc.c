#include "page_alloc.h"

#include "layout.h"

struct free_page {
    struct free_page *next;
};

static struct free_page *free_list;
static size_t free_pages;
static size_t total_pages;

static uint64_t align_up(uint64_t value)
{
    if (value > UINT64_MAX - PAGE_MASK)
        return UINT64_MAX & ~PAGE_MASK;
    return (value + PAGE_MASK) & ~PAGE_MASK;
}

static bool overlaps(uint64_t base, uint64_t end, uint64_t other_base,
                     uint64_t other_size)
{
    uint64_t other_end;

    if (other_size == 0)
        return false;
    other_end = other_base > UINT64_MAX - other_size
                    ? UINT64_MAX
                    : other_base + other_size;
    return base < other_end && other_base < end;
}

static bool page_reserved(const struct boot_info *info, uint64_t base,
                          uint64_t kernel_start, uint64_t kernel_end,
                          uint64_t dtb_start, uint64_t dtb_size,
                          uint64_t firmware_start)
{
    uint64_t end = base + PAGE_SIZE;

    if (overlaps(base, end, kernel_start, kernel_end - kernel_start) ||
        overlaps(base, end, dtb_start, dtb_size) ||
        (base >= firmware_start && base < kernel_start))
        return true;
    for (size_t i = 0; i < info->reserved_count; ++i) {
        if (overlaps(base, end, info->reserved[i].base,
                     info->reserved[i].size))
            return true;
    }
    return false;
}

bool page_allocator_init(const struct boot_info *info, uint64_t kernel_start,
                         uint64_t kernel_end, uint64_t dtb_start,
                         uint64_t dtb_size)
{
    uint64_t firmware_start = kernel_start;

    if (!info || kernel_start >= kernel_end)
        return false;
    free_list = 0;
    free_pages = 0;
    total_pages = 0;

    /* The RAM range containing the loaded kernel also contains OpenSBI below
     * it.  Treat that prefix as firmware-owned even if the DTB omits it. */
    for (size_t i = 0; i < info->memory_count; ++i) {
        uint64_t end = info->memory[i].base + info->memory[i].size;

        if (kernel_start >= info->memory[i].base && kernel_start < end) {
            firmware_start = info->memory[i].base;
            break;
        }
    }

    for (size_t i = 0; i < info->memory_count; ++i) {
        uint64_t base = align_up(info->memory[i].base);
        uint64_t raw_end = info->memory[i].base + info->memory[i].size;
        uint64_t end = raw_end & ~PAGE_MASK;

        if (raw_end < info->memory[i].base)
            return false;
        for (uint64_t page = base; page < end; page += PAGE_SIZE) {
            struct free_page *node;

            if (page_reserved(info, page, kernel_start, kernel_end,
                              dtb_start, dtb_size, firmware_start))
                continue;
            node = phys_to_virt(page);
            node->next = free_list;
            free_list = node;
            ++free_pages;
            ++total_pages;
        }
    }
    return free_pages != 0;
}

uint64_t page_alloc(void)
{
    struct free_page *page = free_list;

    if (!page)
        return 0;
    free_list = page->next;
    --free_pages;
    return virt_to_phys(page);
}

void page_free(uint64_t physical)
{
    struct free_page *page;

    if ((physical & PAGE_MASK) != 0 || physical == 0)
        return;
    page = phys_to_virt(physical);
    page->next = free_list;
    free_list = page;
    ++free_pages;
}

size_t page_free_count(void)
{
    return free_pages;
}

size_t page_total_count(void)
{
    return total_pages;
}
