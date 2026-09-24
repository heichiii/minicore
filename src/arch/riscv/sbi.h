#ifndef MINICORE_RISCV_SBI_H
#define MINICORE_RISCV_SBI_H

#include <stdbool.h>
#include <stdint.h>

#define SBI_EXT_TIME UINT64_C(0x54494d45)

bool sbi_probe_extension(uint64_t extension_id);
void sbi_set_timer(uint64_t absolute_time);

#endif
