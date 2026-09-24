#ifndef MINICORE_RISCV_CSR_H
#define MINICORE_RISCV_CSR_H

#include <stdint.h>

#define SCAUSE_INTERRUPT (UINT64_C(1) << 63)

#define SSTATUS_SIE (UINT64_C(1) << 1)
#define SSTATUS_SPIE (UINT64_C(1) << 5)
#define SSTATUS_SPP (UINT64_C(1) << 8)
#define SSTATUS_SUM (UINT64_C(1) << 18)
#define SIE_STIE (UINT64_C(1) << 5)

static inline uint64_t csr_read_time(void)
{
    uint64_t value;

    __asm__ volatile("rdtime %0" : "=r"(value));
    return value;
}

static inline void csr_write_stvec(uint64_t value)
{
    __asm__ volatile("csrw stvec, %0" : : "r"(value) : "memory");
}

static inline void csr_set_sstatus(uint64_t bits)
{
    __asm__ volatile("csrs sstatus, %0" : : "r"(bits) : "memory");
}

static inline void csr_clear_sstatus(uint64_t bits)
{
    __asm__ volatile("csrc sstatus, %0" : : "r"(bits) : "memory");
}

static inline void csr_set_sie(uint64_t bits)
{
    __asm__ volatile("csrs sie, %0" : : "r"(bits) : "memory");
}

static inline void csr_clear_sie(uint64_t bits)
{
    __asm__ volatile("csrc sie, %0" : : "r"(bits) : "memory");
}

static inline uint64_t csr_read_satp(void)
{
    uint64_t value;

    __asm__ volatile("csrr %0, satp" : "=r"(value));
    return value;
}

#endif
