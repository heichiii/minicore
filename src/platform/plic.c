#include "plic.h"

#include <stddef.h>

/* QEMU virt assigns M/S contexts consecutively for each hart. */
#define PLIC_ENABLE_BASE UINT64_C(0x2000)
#define PLIC_ENABLE_STRIDE UINT64_C(0x80)
#define PLIC_CONTEXT_BASE UINT64_C(0x200000)
#define PLIC_CONTEXT_STRIDE UINT64_C(0x1000)

static uintptr_t plic_base;
static uint32_t plic_sources;
static uint64_t plic_context;

static volatile uint32_t *plic_register(uint64_t offset)
{
    return (volatile uint32_t *)(plic_base + offset);
}

bool plic_init(const struct boot_info *info, uint64_t hart_id)
{
    if (!info || !info->plic_present || info->plic.size < 0x210000 ||
        hart_id > (UINT64_MAX - 1) / 2)
        return false;
    plic_base = (uintptr_t)info->plic.base;
    plic_sources = info->plic_source_count;
    plic_context = hart_id * 2 + 1;
    *plic_register(PLIC_CONTEXT_BASE + plic_context * PLIC_CONTEXT_STRIDE) = 0;
    return true;
}

bool plic_enable(uint32_t source, uint32_t priority)
{
    volatile uint32_t *enable;

    if (plic_base == 0 || source == 0 || source > plic_sources)
        return false;
    *plic_register((uint64_t)source * 4) = priority;
    enable = plic_register(PLIC_ENABLE_BASE + plic_context * PLIC_ENABLE_STRIDE +
                           (source / 32U) * 4U);
    *enable |= UINT32_C(1) << (source % 32U);
    return true;
}

void plic_disable(uint32_t source)
{
    volatile uint32_t *enable;

    if (plic_base == 0 || source == 0 || source > plic_sources)
        return;
    enable = plic_register(PLIC_ENABLE_BASE + plic_context * PLIC_ENABLE_STRIDE +
                           (source / 32U) * 4U);
    *enable &= ~(UINT32_C(1) << (source % 32U));
}

uint32_t plic_claim(void)
{
    if (plic_base == 0)
        return 0;
    return *plic_register(PLIC_CONTEXT_BASE +
                          plic_context * PLIC_CONTEXT_STRIDE + 4);
}

void plic_complete(uint32_t source)
{
    if (plic_base != 0 && source != 0)
        *plic_register(PLIC_CONTEXT_BASE +
                       plic_context * PLIC_CONTEXT_STRIDE + 4) = source;
}
