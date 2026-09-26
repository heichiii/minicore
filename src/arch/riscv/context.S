#define CTX_RA 0
#define CTX_SP 8
#define CTX_S0 16

    .section .text, "ax", @progbits
    .align 2
    .globl context_switch
    .type context_switch, @function
context_switch:
    sd ra, CTX_RA(a0)
    sd sp, CTX_SP(a0)
    sd s0, CTX_S0 + 0*8(a0)
    sd s1, CTX_S0 + 1*8(a0)
    sd s2, CTX_S0 + 2*8(a0)
    sd s3, CTX_S0 + 3*8(a0)
    sd s4, CTX_S0 + 4*8(a0)
    sd s5, CTX_S0 + 5*8(a0)
    sd s6, CTX_S0 + 6*8(a0)
    sd s7, CTX_S0 + 7*8(a0)
    sd s8, CTX_S0 + 8*8(a0)
    sd s9, CTX_S0 + 9*8(a0)
    sd s10, CTX_S0 + 10*8(a0)
    sd s11, CTX_S0 + 11*8(a0)

    ld ra, CTX_RA(a1)
    ld sp, CTX_SP(a1)
    ld s0, CTX_S0 + 0*8(a1)
    ld s1, CTX_S0 + 1*8(a1)
    ld s2, CTX_S0 + 2*8(a1)
    ld s3, CTX_S0 + 3*8(a1)
    ld s4, CTX_S0 + 4*8(a1)
    ld s5, CTX_S0 + 5*8(a1)
    ld s6, CTX_S0 + 6*8(a1)
    ld s7, CTX_S0 + 7*8(a1)
    ld s8, CTX_S0 + 8*8(a1)
    ld s9, CTX_S0 + 9*8(a1)
    ld s10, CTX_S0 + 10*8(a1)
    ld s11, CTX_S0 + 11*8(a1)
    ret
    .size context_switch, . - context_switch

    .globl scheduler_register_probe
    .type scheduler_register_probe, @function
scheduler_register_probe:
    addi sp, sp, -112
    sd ra, 0(sp)
    sd s0, 8(sp)
    sd s1, 16(sp)
    sd s2, 24(sp)
    sd s3, 32(sp)
    sd s4, 40(sp)
    sd s5, 48(sp)
    sd s6, 56(sp)
    sd s7, 64(sp)
    sd s8, 72(sp)
    sd s9, 80(sp)
    sd s10, 88(sp)
    sd s11, 96(sp)
    sd a0, 104(sp)
    li s0, 0x20
    li s1, 0x21
    li s2, 0x22
    li s3, 0x23
    li s4, 0x24
    li s5, 0x25
    li s6, 0x26
    li s7, 0x27
    li s8, 0x28
    li s9, 0x29
    li s10, 0x2a
    li s11, 0x2b
.Lprobe_loop:
    call scheduler_yield
    ld t0, 104(sp)
    addi t0, t0, -1
    sd t0, 104(sp)
    bnez t0, .Lprobe_loop
    li a0, 0
    li t0, 0x20
    bne s0, t0, .Lprobe_done
    li t0, 0x21
    bne s1, t0, .Lprobe_done
    li t0, 0x22
    bne s2, t0, .Lprobe_done
    li t0, 0x23
    bne s3, t0, .Lprobe_done
    li t0, 0x24
    bne s4, t0, .Lprobe_done
    li t0, 0x25
    bne s5, t0, .Lprobe_done
    li t0, 0x26
    bne s6, t0, .Lprobe_done
    li t0, 0x27
    bne s7, t0, .Lprobe_done
    li t0, 0x28
    bne s8, t0, .Lprobe_done
    li t0, 0x29
    bne s9, t0, .Lprobe_done
    li t0, 0x2a
    bne s10, t0, .Lprobe_done
    li t0, 0x2b
    bne s11, t0, .Lprobe_done
    li a0, 1
.Lprobe_done:
    ld ra, 0(sp)
    ld s0, 8(sp)
    ld s1, 16(sp)
    ld s2, 24(sp)
    ld s3, 32(sp)
    ld s4, 40(sp)
    ld s5, 48(sp)
    ld s6, 56(sp)
    ld s7, 64(sp)
    ld s8, 72(sp)
    ld s9, 80(sp)
    ld s10, 88(sp)
    ld s11, 96(sp)
    addi sp, sp, 112
    ret
    .size scheduler_register_probe, . - scheduler_register_probe

    .section .note.GNU-stack, "", @progbits
