#ifndef MINICORE_ADDRESS_SPACE_H
#define MINICORE_ADDRESS_SPACE_H

#include <stddef.h>
#include "vm.h"
#include "../kernel/list.h"
#include "../abi.h"

enum area_kind { AREA_ELF, AREA_STACK, AREA_HEAP, AREA_ANON, AREA_FILE };
/* VMA records describe reservations, even PROT_NONE with no hardware leaf.
 * Ranges are page-aligned half-open intervals. Page ownership remains in the
 * separate page ledger; splitting a VMA never duplicates physical storage. */
struct vm_area {
    struct list_node link;
    uint64_t start, end, flags;
    enum area_kind kind;
};
struct file;

/*
 * A process address space combines three kinds of state that have different
 * jobs:
 *
 *   table  - the Sv39 translation structure consumed by the CPU;
 *   pages  - the kernel's ownership ledger for user data/code pages.
 *   areas  - virtual reservations and policy, including inaccessible regions.
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
    struct list_node areas;
    uint64_t brk_base, brk_end;
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

/* Eagerly duplicate every owned user page into a new address space. */
bool address_space_clone(struct address_space *destination,
                         const struct address_space *source,
                         const struct page_table *kernel_table);

/* Transfer all ownership from source into an empty destination. */
void address_space_move(struct address_space *destination,
                        struct address_space *source);

/*
 * Copy size bytes between a kernel buffer and user virtual memory.  When
 * to_user is true, buffer is the source and address is the destination;
 * otherwise address is the source and buffer is the destination.  The copy
 * validates every page and may span page boundaries.  It never dereferences a
 * user virtual address directly, so SUM can remain clear.
 */
bool address_space_copy(struct address_space *as, void *buffer,
                        uint64_t address, size_t size, bool to_user);
bool address_space_validate(struct address_space *, uint64_t, size_t, bool);
/* Reserve only metadata; callers subsequently populate the range with pages.
 * Every interval is disjoint. Failure leaves existing reservations intact. */
bool address_space_reserve(struct address_space *, uint64_t, uint64_t,
                           uint64_t, enum area_kind);
/* brk(0) queries; other requests eagerly grow/shrink the fixed heap. The exact
 * byte break is recorded, but hardware protection remains page-granular. */
int64_t address_space_brk(struct address_space *, uint64_t);
/* Eager, private mappings only. File contents are copied without advancing its
 * offset, so no file reference is needed after a successful mmap. */
int64_t address_space_mmap(struct address_space *, uint64_t, uint64_t,
                           unsigned, unsigned, struct file *, uint64_t);
/* Round length up; reject ELF/stack/heap intersections before any mutation.
 * A middle cut splits one VMA, but preserves the surviving physical pages. */
int64_t address_space_munmap(struct address_space *, uint64_t, uint64_t);
#endif
