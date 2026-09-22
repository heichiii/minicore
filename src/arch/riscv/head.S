    .section .text.boot, "ax", @progbits
    .align 2
    .globl _start
    .type _start, @function
_start:
    /* OpenSBI supplies a0=hart ID and a1=DTB physical address. */
    mv s0, a0
    mv s1, a1

    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop

    la sp, __boot_stack_top

    /* The ELF loader does not materialize a NOLOAD .bss section. */
    la t0, __bss_start
    la t1, __bss_end
.Lclear_bss:
    bgeu t0, t1, .Lbss_done
    sd zero, 0(t0)
    addi t0, t0, 8
    j .Lclear_bss
.Lbss_done:
    mv a0, s0
    mv a1, s1
    call kernel_main

    /* kernel_main is not allowed to return. */
    call panic_returned_from_kernel_main
.Lhang:
    wfi
    j .Lhang
    .size _start, . - _start

    .section .note.GNU-stack, "", @progbits
