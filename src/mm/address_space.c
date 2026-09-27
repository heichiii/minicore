#include "address_space.h"
#include "heap.h"
#include "layout.h"
#include "page_alloc.h"
#include "../runtime.h"

/*
 * Software ownership record for one mapped user leaf.  vm_destroy_user()
 * frees private page-table pages, while these records are what let us free
 * the physical pages stored in leaf PTEs.  virtual_address and flags are kept
 * for the later eager-copy fork implementation as well as diagnostics.
 */
struct user_page {
    struct list_node link;
    uint64_t virtual_address;
    uint64_t physical;
    uint64_t flags;
};

bool address_space_init(struct address_space *as,
                        const struct page_table *kernel_table)
{
    /* Leave a destructible object even if vm_create_user() fails part-way. */
    as->table.root = 0;
    list_init(&as->pages);
    return vm_create_user(&as->table, kernel_table);
}

void address_space_destroy(struct address_space *as)
{
    /*
     * vm_destroy_user() walks only lower-half page-table nodes.  It must not
     * follow or free the borrowed upper-half kernel mappings, and it does not
     * own leaf backing pages.  The ledger below releases those backing pages.
     */
    vm_destroy_user(&as->table);
    while (!list_empty(&as->pages)) {
        struct user_page *page = container_of(as->pages.next,
                                              struct user_page, link);
        list_remove(&page->link);
        page_free(page->physical);
        kfree(page);
    }
}

bool address_space_add_page(struct address_space *as, uint64_t address,
                            uint64_t flags, const void *data, size_t size)
{
    struct user_page *page;

    /* Sv39's canonical lower half ends before bit 38 becomes one. */
    if (address >= (UINT64_C(1) << 38) || (address & PAGE_MASK) ||
        !(flags & PTE_U) || size > PAGE_SIZE || (size && !data))
        return false;

    /* Allocate the ownership record first so no untracked page can exist. */
    page = kmalloc(sizeof(*page));
    if (!page)
        return false;
    page->physical = page_alloc();
    if (!page->physical) {
        kfree(page);
        return false;
    }
    memset(phys_to_virt(page->physical), 0, PAGE_SIZE);
    if (size)
        memcpy(phys_to_virt(page->physical), data, size);

    /*
     * Publish the record only after vm_map() succeeds.  Until then this
     * function retains both allocations and can unwind them locally.
     */
    if (!vm_map(&as->table, address, page->physical, flags)) {
        page_free(page->physical);
        kfree(page);
        return false;
    }
    page->virtual_address = address;
    page->flags = flags;
    list_push_back(&as->pages, &page->link);
    return true;
}

bool address_space_clone(struct address_space *destination,
                         const struct address_space *source,
                         const struct page_table *kernel_table)
{
    if (!address_space_init(destination, kernel_table))
        return false;
    for (struct list_node *entry = source->pages.next;
         entry != &source->pages; entry = entry->next) {
        const struct user_page *page = container_of(entry,
                                                     struct user_page, link);

        if (!address_space_add_page(destination, page->virtual_address,
                                    page->flags,
                                    phys_to_virt(page->physical), PAGE_SIZE)) {
            address_space_destroy(destination);
            return false;
        }
    }
    return true;
}

void address_space_move(struct address_space *destination,
                        struct address_space *source)
{
    destination->table = source->table;
    source->table.root = 0;
    list_init(&destination->pages);
    while (!list_empty(&source->pages)) {
        struct list_node *entry = source->pages.next;

        list_remove(entry);
        list_push_back(&destination->pages, entry);
    }
}

bool address_space_copy(struct address_space *as, void *buffer,
                        uint64_t address, size_t size, bool to_user)
{
    unsigned char *bytes = buffer;
    const uint64_t user_end = UINT64_C(1) << 38;

    /* A zero-length copy is valid even for a null buffer or boundary address. */
    if (!size)
        return true;

    /* size > user_end - address also rejects addition overflow. */
    if (!as || !buffer || address >= user_end || size > user_end - address)
        return false;
    while (size) {
        uint64_t physical, flags;
        size_t chunk = PAGE_SIZE - (address & PAGE_MASK);

        if (chunk > size)
            chunk = size;
        /*
         * Revalidate at each page boundary.  A readable U mapping is required
         * in both directions; writing user memory additionally requires W.
         */
        if (!vm_query(&as->table, address, &physical, &flags) ||
            (flags & (PTE_U | PTE_R)) != (PTE_U | PTE_R) ||
            (to_user && !(flags & PTE_W)))
            return false;

        /* physical includes the original page offset; use the direct map. */
        if (to_user)
            memcpy(phys_to_virt(physical), bytes, chunk);
        else
            memcpy(bytes, phys_to_virt(physical), chunk);
        bytes += chunk;
        address += chunk;
        size -= chunk;
    }
    return true;
}
