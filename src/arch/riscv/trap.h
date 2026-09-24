#ifndef MINICORE_RISCV_TRAP_H
#define MINICORE_RISCV_TRAP_H

#include <stdbool.h>
#include <stdint.h>

struct trap_frame {
    uint64_t x[32];
    uint64_t sepc;
    uint64_t sstatus;
    uint64_t scause;
    uint64_t stval;
};

void trap_init(void);
bool trap_run_m1_tests(uint64_t timebase_frequency);
void trap_dispatch(struct trap_frame *frame);

#endif
