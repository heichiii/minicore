#ifndef MINICORE_ADDRESS_SPACE_H
#define MINICORE_ADDRESS_SPACE_H

#include <stddef.h>
#include "vm.h"
#include "../kernel/list.h"

/*
 * A process address space combines two kinds of state that have different
 * jobs:
 *
 *   table  - the Sv39 translation structure consumed by the CPU;
 *   pages  - the kernel's ownership ledger for user data/code pages.
 *
 * Page-table leaves tell the CPU where a virtual address points, but they do
 * not by themselves say who must free the referenced physical page.  Every
 * physical page allocated by address_space_add_page() therefore also has a
 * user_page entry on pages.  address_space_destroy() uses that ledger to
 * release the leaves' backing storage after destroying the private page-table
 * pages.
 *
 * The lower Sv39 half is private to this address space.  Its upper-half root
 * entries are copied from, and borrow descendants owned by, the permanent
 * kernel page table.  User leaves must carry PTE_U; shared kernel leaves do
 * not, so they remain inaccessible in U-mode.
 */
struct address_space {
    struct page_table table;
    struct list_node pages;
};

/* Create an empty lower half and attach the shared kernel upper half. */
bool address_space_init(struct address_space *as,
                        const struct page_table *kernel_table);

/*
 * Release every resource owned by as.  The caller must already be executing
 * in another address space, and no runnable thread may still refer to as.
 */
void address_space_destroy(struct address_space *as);

/*
 * Allocate and map one zero-filled user page at a page-aligned address.
 * data[0..size) optionally supplies its initial contents.  Ownership of the
 * physical page is transferred to as only after the mapping succeeds; every
 * failure path rolls back its partial allocations.
 */
bool address_space_add_page(struct address_space *as, uint64_t address,
                            uint64_t flags, const void *data, size_t size);

/*
 * Copy size bytes between a kernel buffer and user virtual memory.  When
 * to_user is true, buffer is the source and address is the destination;
 * otherwise address is the source and buffer is the destination.  The copy
 * validates every page and may span page boundaries.  It never dereferences a
 * user virtual address directly, so SUM can remain clear.
 */
bool address_space_copy(struct address_space *as, void *buffer,
                        uint64_t address, size_t size, bool to_user);
#endif
