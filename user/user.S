    .section .text, "ax", @progbits
    .globl _start
_start:
    li t0, 3
    bgeu a0, t0, .Lthreads
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
    li a7, 3
    ecall
    bnez a0, .Lfailed

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

/* Modes 3/4 own different contents at identical virtual addresses.  Mode 5
 * crashes while they are runnable; neither survivor may be disturbed. */
.Lthreads:
    li t0, 5
    beq a0, t0, .Lillegal
    mv s0, a0
    li s1, 4
    li t0, 0x3fe000
    sd s0, 0(t0)
.Lthread_round:
    call .Lregister_probe
    li a7, 3
    ecall
    bnez a0, .Lfailed
    li t0, 0x3fe000
    ld t1, 0(t0)
    bne t1, s0, .Lfailed
    addi s1, s1, -1
    bnez s1, .Lthread_round
    li a0, 0
    li a7, 2
    ecall
.Lillegal:
    .word 0
    j .Lfailed

.Lregister_probe:
    addi sp, sp, -64
    sd ra, 0(sp)
    sd s0, 8(sp)
    sd s1, 16(sp)
    sd s0, 24(sp)
    /* Leave every integer register except sp and the loop counter at a
     * distinct known value across timer-only preemption. */
    .irp reg,1,3,4,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    ld t0, 8(sp)
    addi x\reg, t0, \reg
    .endr
    li t0, 16000000
.Lbusy:
    addi t0, t0, -1
    bnez t0, .Lbusy
    .irp reg,1,3,4,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    ld t0, 8(sp)
    addi t0, t0, \reg
    bne x\reg, t0, .Lfailed
    .endr
    /* A syscall may change a0 (result) and a7 (our call number).  Every
     * other register must survive an explicit scheduler switch as well. */
    li a7, 3
    ecall
    bnez a0, .Lfailed
    .irp reg,1,3,4,6,7,8,9,11,12,13,14,15,16,18,19,20,21,22,23,24,25,26,27,28,29,30,31
    ld t0, 8(sp)
    addi t0, t0, \reg
    bne x\reg, t0, .Lfailed
    .endr
    li t0, 0x3fffb0
    bne sp, t0, .Lfailed
    ld t0, 8(sp)
    ld t1, 24(sp)
    bne t0, t1, .Lfailed
    li t1, 0x3fe000
    ld t1, 0(t1)
    bne t0, t1, .Lfailed
    ld ra, 0(sp)
    ld s0, 8(sp)
    ld s1, 16(sp)
    addi sp, sp, 64
    ret
