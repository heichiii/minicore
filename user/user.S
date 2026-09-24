    .section .text, "ax", @progbits
    .globl _start
_start:
    beqz a0, .Lnormal
    li t0, 1
    beq a0, t0, .Lkernel_access

    /* The program text is U+RX, never writable. */
    la t0, _start
    sd zero, 0(t0)
    j .Lfailed

.Lkernel_access:
    /* The kernel direct map exists in this page table but lacks PTE_U. */
    li t0, 0xffffffc080200000
    ld t1, 0(t0)
    j .Lfailed

.Lnormal:
    /* Put eight bytes across the boundary between the two stack pages. */
    li t0, 0x3feffc
    li t1, 'M'
    sb t1, 0(t0)
    li t1, '3'
    sb t1, 1(t0)
    li t1, ' '
    sb t1, 2(t0)
    li t1, 'U'
    sb t1, 3(t0)
    li t1, 'S'
    sb t1, 4(t0)
    li t1, 'E'
    sb t1, 5(t0)
    li t1, 'R'
    sb t1, 6(t0)
    li t1, '\n'
    sb t1, 7(t0)
    li a0, 1
    mv a1, t0
    li a2, 8
    li a7, 1
    ecall
    li t0, 8
    bne a0, t0, .Lfailed

    /* An unmapped pointer must become -EFAULT, not a supervisor fault. */
    li a0, 1
    li a1, 0x500000
    li a2, 1
    li a7, 1
    ecall
    li t0, -14
    bne a0, t0, .Lfailed

    li a0, 0
    li a7, 2
    ecall

.Lfailed:
    li a0, 1
    li a7, 2
    ecall
.Lhang:
    j .Lhang
