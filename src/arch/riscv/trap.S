#define TF_X(n) ((n) * 8)
#define TF_SEPC (32 * 8)
#define TF_SSTATUS (33 * 8)
#define TF_SCAUSE (34 * 8)
#define TF_STVAL (35 * 8)
#define TF_SIZE (36 * 8)

    .section .text.trap, "ax", @progbits
    .align 2
    .globl trap_entry
    .type trap_entry, @function
trap_entry:
    addi sp, sp, -TF_SIZE
    sd zero, TF_X(0)(sp)
    sd ra, TF_X(1)(sp)
    sd t0, TF_X(5)(sp)
    addi t0, sp, TF_SIZE
    sd t0, TF_X(2)(sp)
    sd gp, TF_X(3)(sp)
    sd tp, TF_X(4)(sp)
    sd t1, TF_X(6)(sp)
    sd t2, TF_X(7)(sp)
    sd s0, TF_X(8)(sp)
    sd s1, TF_X(9)(sp)
    sd a0, TF_X(10)(sp)
    sd a1, TF_X(11)(sp)
    sd a2, TF_X(12)(sp)
    sd a3, TF_X(13)(sp)
    sd a4, TF_X(14)(sp)
    sd a5, TF_X(15)(sp)
    sd a6, TF_X(16)(sp)
    sd a7, TF_X(17)(sp)
    sd s2, TF_X(18)(sp)
    sd s3, TF_X(19)(sp)
    sd s4, TF_X(20)(sp)
    sd s5, TF_X(21)(sp)
    sd s6, TF_X(22)(sp)
    sd s7, TF_X(23)(sp)
    sd s8, TF_X(24)(sp)
    sd s9, TF_X(25)(sp)
    sd s10, TF_X(26)(sp)
    sd s11, TF_X(27)(sp)
    sd t3, TF_X(28)(sp)
    sd t4, TF_X(29)(sp)
    sd t5, TF_X(30)(sp)
    sd t6, TF_X(31)(sp)

    csrr t0, sepc
    sd t0, TF_SEPC(sp)
    csrr t0, sstatus
    sd t0, TF_SSTATUS(sp)
    csrr t0, scause
    sd t0, TF_SCAUSE(sp)
    csrr t0, stval
    sd t0, TF_STVAL(sp)

    /* C code must not inherit a deliberately clobbered or future user gp. */
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    mv a0, sp
    call trap_dispatch

    ld t0, TF_SEPC(sp)
    csrw sepc, t0
    ld t0, TF_SSTATUS(sp)
    csrw sstatus, t0

    ld ra, TF_X(1)(sp)
    ld gp, TF_X(3)(sp)
    ld tp, TF_X(4)(sp)
    ld t0, TF_X(5)(sp)
    ld t1, TF_X(6)(sp)
    ld t2, TF_X(7)(sp)
    ld s0, TF_X(8)(sp)
    ld s1, TF_X(9)(sp)
    ld a0, TF_X(10)(sp)
    ld a1, TF_X(11)(sp)
    ld a2, TF_X(12)(sp)
    ld a3, TF_X(13)(sp)
    ld a4, TF_X(14)(sp)
    ld a5, TF_X(15)(sp)
    ld a6, TF_X(16)(sp)
    ld a7, TF_X(17)(sp)
    ld s2, TF_X(18)(sp)
    ld s3, TF_X(19)(sp)
    ld s4, TF_X(20)(sp)
    ld s5, TF_X(21)(sp)
    ld s6, TF_X(22)(sp)
    ld s7, TF_X(23)(sp)
    ld s8, TF_X(24)(sp)
    ld s9, TF_X(25)(sp)
    ld s10, TF_X(26)(sp)
    ld s11, TF_X(27)(sp)
    ld t3, TF_X(28)(sp)
    ld t4, TF_X(29)(sp)
    ld t5, TF_X(30)(sp)
    ld t6, TF_X(31)(sp)
    ld sp, TF_X(2)(sp)
    sret
    .size trap_entry, . - trap_entry

    .section .text, "ax", @progbits
    .align 2
    .globl trap_trigger_test_exceptions
    .type trap_trigger_test_exceptions, @function
trap_trigger_test_exceptions:
    csrw cycle, zero
    ebreak
    ld zero, 0(zero)
    ret
    .size trap_trigger_test_exceptions, . - trap_trigger_test_exceptions

    .align 2
    .globl trap_register_test
    .type trap_register_test, @function
trap_register_test:
    addi sp, sp, -128
    sd ra, 0(sp)
    sd gp, 8(sp)
    sd tp, 16(sp)
    sd s0, 24(sp)
    sd s1, 32(sp)
    sd s2, 40(sp)
    sd s3, 48(sp)
    sd s4, 56(sp)
    sd s5, 64(sp)
    sd s6, 72(sp)
    sd s7, 80(sp)
    sd s8, 88(sp)
    sd s9, 96(sp)
    sd s10, 104(sp)
    sd s11, 112(sp)
    .option push
    .option norelax
    la t0, trap_register_test_sp
    .option pop
    sd sp, 0(t0)

    li ra, 1
    li gp, 3
    li tp, 4
    li t0, 5
    li t1, 6
    li t2, 7
    li s0, 8
    li s1, 9
    li a0, 10
    li a1, 11
    li a2, 12
    li a3, 13
    li a4, 14
    li a5, 15
    li a6, 16
    li a7, 17
    li s2, 18
    li s3, 19
    li s4, 20
    li s5, 21
    li s6, 22
    li s7, 23
    li s8, 24
    li s9, 25
    li s10, 26
    li s11, 27
    li t3, 28
    li t4, 29
    li t5, 30
    li t6, 31
    .globl trap_register_window_start
trap_register_window_start:
    wfi
    j trap_register_window_start

    .globl trap_register_window_end
trap_register_window_end:
    .option push
    .option norelax
    la t0, trap_register_test_sp
    .option pop
    ld sp, 0(t0)
    ld ra, 0(sp)
    ld gp, 8(sp)
    ld tp, 16(sp)
    ld s0, 24(sp)
    ld s1, 32(sp)
    ld s2, 40(sp)
    ld s3, 48(sp)
    ld s4, 56(sp)
    ld s5, 64(sp)
    ld s6, 72(sp)
    ld s7, 80(sp)
    ld s8, 88(sp)
    ld s9, 96(sp)
    ld s10, 104(sp)
    ld s11, 112(sp)
    addi sp, sp, 128
    ret
    .size trap_register_test, . - trap_register_test

    .section .note.GNU-stack, "", @progbits
