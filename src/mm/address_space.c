#include "address_space.h"
#include "heap.h"
#include "layout.h"
#include "page_alloc.h"
#include "../runtime.h"
#include "../fs/vfs.h"

#define HEAP_BASE UINT64_C(0x800000)
#define MMAP_BASE UINT64_C(0x10000000)
#define MMAP_END UINT64_C(0x20000000)
#define MAX_MAPPING UINT64_C(0x1000000)

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
    list_init(&as->areas);
    as->brk_base = as->brk_end = HEAP_BASE;
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
    while (!list_empty(&as->areas)) {
        struct vm_area *area = container_of(as->areas.next, struct vm_area, link);
        list_remove(&area->link);
        kfree(area);
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
    /* PROT_NONE reserves and owns storage without installing an invalid R/W/X
     * leaf. Access faults naturally; munmap and fork still find the ledger. */
    if ((flags & (PTE_R | PTE_X)) &&
        !vm_map(&as->table, address, page->physical, flags)) {
        page_free(page->physical);
        kfree(page);
        vm_prune_user(&as->table);
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
    destination->brk_base = source->brk_base;
    destination->brk_end = source->brk_end;
    for (struct list_node *entry = source->areas.next;
         entry != &source->areas; entry = entry->next) {
        const struct vm_area *area = container_of(entry, struct vm_area, link);
        if (!address_space_reserve(destination, area->start, area->end,
                                    area->flags, area->kind)) {
            address_space_destroy(destination);
            return false;
        }
    }
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
    destination->brk_base = source->brk_base;
    destination->brk_end = source->brk_end;
    source->table.root = 0;
    list_init(&destination->pages);
    while (!list_empty(&source->pages)) {
        struct list_node *entry = source->pages.next;

        list_remove(entry);
        list_push_back(&destination->pages, entry);
    }
    list_init(&destination->areas);
    while (!list_empty(&source->areas)) {
        struct list_node *entry = source->areas.next;
        list_remove(entry);
        list_push_back(&destination->areas, entry);
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

/* Validate the entire range before any I/O consumes bytes or advances an
 * offset. One thread per process and no shared VM mean the mappings remain
 * stable across a blocking pipe read; no page pinning is needed in M6. */
bool address_space_validate(struct address_space *as, uint64_t address,
                             size_t size, bool write)
{
    if (!size) return true;
    const uint64_t end = UINT64_C(1) << 38;
    if (address >= end || size > end - address) return false;
    while (size) {
        uint64_t flags;
        size_t part = PAGE_SIZE - (address & PAGE_MASK);
        if (part > size) part = size;
        if (!vm_query(&as->table, address, 0, &flags) ||
            (flags & (PTE_U | PTE_R)) != (PTE_U | PTE_R) ||
            (write && !(flags & PTE_W))) return false;
        address += part; size -= part;
    }
    return true;
}

static bool vacant(struct address_space *as, uint64_t start, uint64_t end)
{
    for (struct list_node *e = as->areas.next; e != &as->areas; e = e->next) {
        struct vm_area *a = container_of(e, struct vm_area, link);
        if (start < a->end && a->start < end) return false;
    }
    return true;
}

bool address_space_reserve(struct address_space *as, uint64_t start,
                           uint64_t end, uint64_t flags, enum area_kind kind)
{
    if (start >= end || (start & PAGE_MASK) || (end & PAGE_MASK) ||
        end > (UINT64_C(1) << 38) || !vacant(as, start, end)) return false;
    struct vm_area *a = kmalloc(sizeof(*a));
    if (!a) return false;
    a->start = start; a->end = end; a->flags = flags; a->kind = kind;
    list_push_back(&as->areas, &a->link);
    return true;
}

static void remove_pages(struct address_space *as, uint64_t start, uint64_t end)
{
    struct list_node *e = as->pages.next;
    while (e != &as->pages) {
        struct user_page *p = container_of(e, struct user_page, link);
        e = e->next;
        if (p->virtual_address < start || p->virtual_address >= end) continue;
        if (p->flags & (PTE_R | PTE_X)) vm_unmap(&as->table, p->virtual_address);
        list_remove(&p->link); page_free(p->physical); kfree(p);
    }
    vm_prune_user(&as->table);
}

int64_t address_space_mmap(struct address_space *as, uint64_t address,
                           uint64_t length, unsigned prot, unsigned mapflags,
                           struct file *file, uint64_t offset)
{
    unsigned allowed = MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE;
    if (!length || length > MAX_MAPPING || (prot & ~7U) ||
        ((prot & PROT_WRITE) && !(prot & PROT_READ)) ||
        (prot & (PROT_WRITE | PROT_EXEC)) == (PROT_WRITE | PROT_EXEC) ||
        !(mapflags & MAP_PRIVATE) || (mapflags & ~allowed) ||
        (offset & PAGE_MASK) || ((mapflags & MAP_ANONYMOUS) && offset)) return -E_INVAL;
    if (!(mapflags & MAP_ANONYMOUS) &&
        (!file || !file->node || file->node->type != DT_REG ||
         (file->flags & O_ACCMODE) == O_WRONLY)) return -E_BADF;
    length = (length + PAGE_MASK) & ~PAGE_MASK;
    if (offset > UINT64_MAX - length) return -E_INVAL;
    if (address & PAGE_MASK) return -E_INVAL;
    /* Fixed mappings never overwrite. Nonfixed requests are hints and fall
     * back to a first-fit scan; all searches consult VMAs, including NONE. */
    if (mapflags & MAP_FIXED_NOREPLACE) {
        if (address < MMAP_BASE || address > MMAP_END - length) return -E_INVAL;
        if (!vacant(as, address, address + length)) return -E_EXIST;
    } else if (address < MMAP_BASE || address > MMAP_END - length ||
               !vacant(as, address, address + length)) {
        for (address = MMAP_BASE; address <= MMAP_END - length; address += PAGE_SIZE)
            if (vacant(as, address, address + length)) break;
        if (address > MMAP_END - length) return -E_NOMEM;
    }
    uint64_t flags = PTE_U | ((prot & PROT_READ) ? PTE_R : 0) |
                     ((prot & PROT_WRITE) ? PTE_W : 0) | ((prot & PROT_EXEC) ? PTE_X : 0);
    if (!address_space_reserve(as, address, address + length, flags,
                              (mapflags & MAP_ANONYMOUS) ? AREA_ANON : AREA_FILE)) return -E_NOMEM;
    struct vm_area *area = container_of(as->areas.prev, struct vm_area, link);
    for (uint64_t at = address; at < address + length; at += PAGE_SIZE) {
        if (!address_space_add_page(as, at, flags, 0, 0)) goto fail;
        if (!(mapflags & MAP_ANONYMOUS)) {
            struct user_page *page = container_of(as->pages.prev, struct user_page, link);
            /* Eager MAP_PRIVATE snapshot. Closing the fd is safe afterwards;
             * mutations in this mapping never write back to the file. */
            if (file_pread(file, phys_to_virt(page->physical), PAGE_SIZE,
                           offset + at - address) < 0) goto fail;
        }
    }
    vm_flush_all();
    if (prot & PROT_EXEC) __asm__ volatile("fence.i" : : : "memory");
    return (int64_t)address;
fail:
    remove_pages(as, address, address + length);
    list_remove(&area->link); kfree(area);
    return -E_NOMEM;
}

int64_t address_space_munmap(struct address_space *as, uint64_t start, uint64_t length)
{
    const uint64_t limit = UINT64_C(1) << 38;
    if ((start & PAGE_MASK) || !length || start >= limit ||
        length > limit - start) return -E_INVAL;
    uint64_t end = (start + length + PAGE_MASK) & ~PAGE_MASK;
    struct vm_area *split = 0;
    /* Preflight all intersecting regions before freeing a single page. A
     * middle cut needs one extra VMA; allocate it now so ENOMEM is atomic. */
    for (struct list_node *e = as->areas.next; e != &as->areas; e = e->next) {
        struct vm_area *a = container_of(e, struct vm_area, link);
        if (start >= a->end || end <= a->start) continue;
        if (a->kind != AREA_ANON && a->kind != AREA_FILE) { kfree(split); return -E_PERM; }
        if (start > a->start && end < a->end) {
            split = kmalloc(sizeof(*split));
            if (!split) return -E_NOMEM;
        }
    }
    remove_pages(as, start, end);
    struct list_node *e = as->areas.next;
    while (e != &as->areas) {
        struct vm_area *a = container_of(e, struct vm_area, link);
        e = e->next;
        if (start >= a->end || end <= a->start) continue;
        if (start <= a->start && end >= a->end) { list_remove(&a->link); kfree(a); }
        else if (start > a->start && end < a->end) {
            *split = *a; split->start = end; a->end = start;
            list_push_back(&as->areas, &split->link); split = 0;
        } else if (start <= a->start) a->start = end;
        else a->end = start;
    }
    kfree(split);
    return 0;
}

int64_t address_space_brk(struct address_space *as, uint64_t requested)
{
    if (!requested) return (int64_t)as->brk_end;
    if (requested < as->brk_base || requested > as->brk_base + MAX_MAPPING) return -E_NOMEM;
    uint64_t old = (as->brk_end + PAGE_MASK) & ~PAGE_MASK;
    uint64_t end = (requested + PAGE_MASK) & ~PAGE_MASK;
    struct vm_area *heap = 0;
    for (struct list_node *e = as->areas.next; e != &as->areas; e = e->next) {
        struct vm_area *a = container_of(e, struct vm_area, link);
        if (a->kind == AREA_HEAP) heap = a;
    }
    /* Publish a larger heap boundary only after every new page exists. If a
     * later allocation fails, remove only [old,end), retaining the old heap. */
    if (end > old) {
        if (!vacant(as, old, end)) return -E_NOMEM;
        struct vm_area *fresh = heap ? 0 : kmalloc(sizeof(*fresh));
        if (!heap && !fresh) return -E_NOMEM;
        for (uint64_t at = old; at < end; at += PAGE_SIZE) {
            if (!address_space_add_page(as, at, PTE_U | PTE_R | PTE_W, 0, 0)) {
                remove_pages(as, old, end); kfree(fresh); return -E_NOMEM;
            }
        }
        if (!heap) {
            heap = fresh; heap->start = as->brk_base;
            heap->kind = AREA_HEAP; heap->flags = PTE_U | PTE_R | PTE_W;
            list_push_back(&as->areas, &heap->link);
        }
        heap->end = end;
        vm_flush_all();
    } else if (end < old) {
        remove_pages(as, end, old);
        if (heap && end == heap->start) { list_remove(&heap->link); kfree(heap); }
        else if (heap) heap->end = end;
    }
    as->brk_end = requested;
    return (int64_t)requested;
}
