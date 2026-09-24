#include "sbi.h"

#define SBI_EXT_BASE UINT64_C(0x10)
#define SBI_BASE_PROBE_EXTENSION UINT64_C(3)
#define SBI_TIME_SET_TIMER UINT64_C(0)

struct sbi_return {
    long error;
    long value;
};

static struct sbi_return sbi_call(uint64_t extension, uint64_t function,
                                  uint64_t arg0, uint64_t arg1,
                                  uint64_t arg2)
{
    register uint64_t a0 __asm__("a0") = arg0;
    register uint64_t a1 __asm__("a1") = arg1;
    register uint64_t a2 __asm__("a2") = arg2;
    register uint64_t a6 __asm__("a6") = function;
    register uint64_t a7 __asm__("a7") = extension;

    __asm__ volatile("ecall"
                     : "+r"(a0), "+r"(a1)
                     : "r"(a2), "r"(a6), "r"(a7)
                     : "memory");
    return (struct sbi_return){(long)a0, (long)a1};
}

bool sbi_probe_extension(uint64_t extension_id)
{
    struct sbi_return result =
        sbi_call(SBI_EXT_BASE, SBI_BASE_PROBE_EXTENSION, extension_id, 0, 0);

    return result.error == 0 && result.value != 0;
}

void sbi_set_timer(uint64_t absolute_time)
{
    (void)sbi_call(SBI_EXT_TIME, SBI_TIME_SET_TIMER, absolute_time, 0, 0);
}
