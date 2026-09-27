    .section .text
    .globl _start
_start:
    ld t0, 0(sp)
    li t1, 2
    bne t0, t1, fail
    ld t0, 8(sp)
    lbu t0, 0(t0)
    li t1, 'c'
    bne t0, t1, fail
    ld t0, 16(sp)
    lbu t0, 0(t0)
    li t1, 'o'
    bne t0, t1, fail
    ld t0, 32(sp)
    lbu t0, 0(t0)
    li t1, 'M'
    bne t0, t1, fail
    li a7, 4
    ecall
    li t0, 1
    ble a0, t0, fail
    li a0, 1
    la a1, message
    li a2, 10
    li a7, 1
    ecall
    li a0, 42
    li a7, 2
    ecall
fail:
    li a0, 98
    li a7, 2
    ecall
1:  j 1b
    .section .rodata
message: .ascii "ELF CHILD\n"
