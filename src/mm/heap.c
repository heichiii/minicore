#include "heap.h"

#include "layout.h"
#include "page_alloc.h"
#include "../kernel/list.h"
#include "../kernel/sync.h"
#include "../runtime.h"

#define SLAB_MAGIC 0x534c4142U
#define CLASS_COUNT 8U

struct free_object {
    struct free_object *next;
};

struct slab {
    uint32_t magic;
    uint16_t class_index;
    uint16_t used;
    struct free_object *free;
    struct list_node link;
};

static const size_t class_sizes[CLASS_COUNT] = {
    16, 32, 64, 128, 256, 512, 1024, 2048
};
static struct list_node classes[CLASS_COUNT];
static struct spinlock heap_lock;

void heap_init(void)
{
    for (size_t i = 0; i < CLASS_COUNT; ++i)
        list_init(&classes[i]);
    spinlock_init(&heap_lock);
}

static struct slab *new_slab(size_t class_index)
{
    uint64_t physical = page_alloc();
    struct slab *slab;
    size_t size = class_sizes[class_index];
    size_t offset = (sizeof(*slab) + size - 1) & ~(size - 1);

    if (!physical)
        return 0;
    slab = phys_to_virt(physical);
    memset(slab, 0, sizeof(*slab));
    slab->magic = SLAB_MAGIC;
    slab->class_index = (uint16_t)class_index;
    list_init(&slab->link);
    for (; offset + size <= PAGE_SIZE; offset += size) {
        struct free_object *object = (void *)((char *)slab + offset);

        object->next = slab->free;
        slab->free = object;
    }
    list_push_back(&classes[class_index], &slab->link);
    return slab;
}

void *kmalloc(size_t size)
{
    struct slab *slab = 0;
    void *result;
    uint64_t flags;
    size_t class_index;

    if (size == 0)
        size = 1;
    for (class_index = 0; class_index < CLASS_COUNT; ++class_index) {
        if (size <= class_sizes[class_index])
            break;
    }
    if (class_index == CLASS_COUNT)
        return 0;
    flags = irq_save();
    spin_lock(&heap_lock);
    for (struct list_node *node = classes[class_index].next;
         node != &classes[class_index]; node = node->next) {
        struct slab *candidate = container_of(node, struct slab, link);

        if (candidate->free) {
            slab = candidate;
            break;
        }
    }
    if (!slab)
        slab = new_slab(class_index);
    if (!slab) {
        spin_unlock(&heap_lock);
        irq_restore(flags);
        return 0;
    }
    result = slab->free;
    slab->free = slab->free->next;
    ++slab->used;
    spin_unlock(&heap_lock);
    irq_restore(flags);
    memset(result, 0, class_sizes[class_index]);
    return result;
}

void kfree(void *pointer)
{
    uintptr_t base;
    struct slab *slab;
    struct free_object *object = pointer;
    uint64_t flags;

    if (!pointer)
        return;
    base = (uintptr_t)pointer & ~(uintptr_t)PAGE_MASK;
    slab = (struct slab *)base;
    flags = irq_save();
    spin_lock(&heap_lock);
    if (slab->magic != SLAB_MAGIC || slab->class_index >= CLASS_COUNT ||
        slab->used == 0) {
        spin_unlock(&heap_lock);
        irq_restore(flags);
        return;
    }
    object->next = slab->free;
    slab->free = object;
    --slab->used;
    if (slab->used == 0) {
        list_remove(&slab->link);
        slab->magic = 0;
        spin_unlock(&heap_lock);
        irq_restore(flags);
        page_free(virt_to_phys(slab));
        return;
    }
    spin_unlock(&heap_lock);
    irq_restore(flags);
}
