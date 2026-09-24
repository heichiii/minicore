#ifndef MINICORE_PLATFORM_PLIC_H
#define MINICORE_PLATFORM_PLIC_H

#include <stdbool.h>
#include <stdint.h>

#include "dtb.h"

bool plic_init(const struct boot_info *info, uint64_t hart_id);
bool plic_enable(uint32_t source, uint32_t priority);
void plic_disable(uint32_t source);
uint32_t plic_claim(void);
void plic_complete(uint32_t source);

#endif
