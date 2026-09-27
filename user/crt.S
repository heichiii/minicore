    .section .text.start
    .globl _start
_start:
    # exec supplies argc, argv[], NULL, envp[], NULL on a 16-byte aligned stack.
    # C user objects use -msmall-data-limit=0, so they never borrow kernel gp.
    ld a0, 0(sp)
    addi a1, sp, 8
    slli t0, a0, 3
    add a2, a1, t0
    addi a2, a2, 8
    call main
    call exit
