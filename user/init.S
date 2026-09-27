    .equ SYS_write, 1
    .equ SYS_exit, 2
    .equ SYS_yield, 3
    .equ SYS_getpid, 4
    .equ SYS_exec, 5
    .equ SYS_waitpid, 6
    .equ SYS_fork, 7

    .section .text
    .globl _start
_start:
    ld t0, 0(sp)
    li t1, 1
    bne t0, t1, fail
    ld t0, 8(sp)
    lbu t0, 0(t0)
    li t1, '/'
    bne t0, t1, fail
    li a7, SYS_getpid
    ecall
    li t0, 1
    bne a0, t0, fail
    la t0, shared_word
    ld t0, 0(t0)
    bnez t0, fail

    /* Checked copy: bad pointers fail and a write may cross a page boundary. */
    li a0, 1
    li a1, 0x500000
    li a2, 1
    li a7, SYS_write
    ecall
    li t0, -14
    bne a0, t0, fail
    li t0, 0x3fcffc
    li t1, 'M'
    sb t1, 0(t0)
    li t1, '3'
    sb t1, 1(t0)
    li t1, 'O'
    sb t1, 2(t0)
    li t1, 'K'
    sb t1, 3(t0)
    li t1, '\n'
    sb t1, 4(t0)
    li a0, 1
    mv a1, t0
    li a2, 5
    li a7, SYS_write
    ecall
    li t0, 5
    bne a0, t0, fail

    /* A failed exec must leave this image alive. */
    la a0, missing_path
    la a1, empty_vector
    la a2, empty_vector
    li a7, SYS_exec
    ecall
    li t0, -2
    bne a0, t0, fail
    la a0, bad_path
    la a1, empty_vector
    la a2, empty_vector
    li a7, SYS_exec
    ecall
    li t0, -2
    bne a0, t0, fail

    /* fork -> child exec -> exit -> parent waitpid. */
    li a7, SYS_fork
    ecall
    beqz a0, exec_child
    bltz a0, fail
    mv s0, a0
    mv a0, s0
    li a1, 0x500000
    li a2, 0
    li a7, SYS_waitpid
    ecall
    li t0, -14
    bne a0, t0, fail
    mv a0, s0
    la a1, wait_status
    li a2, 0
    li a7, SYS_waitpid
    ecall
    bne a0, s0, fail
    la t0, wait_status
    ld t0, 0(t0)
    li t1, 42
    bne t0, t1, fail

    /* Eager fork must give parent and child independent writable pages. */
    la t0, shared_word
    li t1, 100
    sd t1, 0(t0)
    li a7, SYS_fork
    ecall
    beqz a0, fork_child
    bltz a0, fail
    mv s0, a0
    la t0, shared_word
    li t1, 111
    sd t1, 0(t0)
    li a7, SYS_yield
    ecall
    mv a0, s0
    la a1, wait_status
    li a2, 0
    li a7, SYS_waitpid
    ecall
    bne a0, s0, fail
    la t0, shared_word
    ld t0, 0(t0)
    li t1, 111
    bne t0, t1, fail
    la t0, wait_status
    ld t0, 0(t0)
    li t1, 7
    bne t0, t1, fail

    /* User faults are process-local: test RX text, kernel isolation, and exec. */
    la s1, text_fault_path
    call run_fault
    la s1, kernel_fault_path
    call run_fault
    la s1, crash_path
    call run_fault

    /* An exiting parent hands its child to PID 1, which reaps both. */
    li a7, SYS_fork
    ecall
    beqz a0, orphan_parent
    bltz a0, fail
    mv s0, a0
    mv a0, s0
    la a1, wait_status
    li a2, 0
    li a7, SYS_waitpid
    ecall
    bne a0, s0, fail
    la t0, wait_status
    ld t0, 0(t0)
    li t1, 12
    bne t0, t1, fail
    li a0, -1
    la a1, wait_status
    li a2, 0
    li a7, SYS_waitpid
    ecall
    bltz a0, fail
    la t0, wait_status
    ld t0, 0(t0)
    li t1, 13
    bne t0, t1, fail

    li a0, -1
    la a1, wait_status
    li a2, 0
    li a7, SYS_waitpid
    ecall
    li t0, -10
    bne a0, t0, fail

    li a0, 1
    la a1, pass_message
    li a2, 13
    li a7, SYS_write
    ecall
    li a0, 0
    li a7, SYS_exit
    ecall

exec_child:
    la a0, child_path
    la a1, child_argv
    la a2, child_envp
    li a7, SYS_exec
    ecall
    j fail

fork_child:
    la t0, shared_word
    li t1, 222
    sd t1, 0(t0)
    li a7, SYS_yield
    ecall
    la t0, shared_word
    ld t0, 0(t0)
    li t1, 222
    bne t0, t1, fail
    li a0, 7
    li a7, SYS_exit
    ecall

orphan_parent:
    li a7, SYS_fork
    ecall
    beqz a0, orphan_child
    bltz a0, fail
    li a0, 12
    li a7, SYS_exit
    ecall
orphan_child:
    li a7, SYS_yield
    ecall
    li a0, 13
    li a7, SYS_exit
    ecall

run_fault:
    li a7, SYS_fork
    ecall
    beqz a0, 1f
    bltz a0, fail
    mv s0, a0
    mv a0, s0
    la a1, wait_status
    li a2, 0
    li a7, SYS_waitpid
    ecall
    bne a0, s0, fail
    la t0, wait_status
    ld t0, 0(t0)
    li t1, -1
    bne t0, t1, fail
    ret
1:
    mv a0, s1
    la a1, empty_vector
    la a2, empty_vector
    li a7, SYS_exec
    ecall
    j fail

fail:
    li a0, 99
    li a7, SYS_exit
    ecall
1:  j 1b

    .section .rodata
missing_path:      .asciz "/missing"
bad_path:          .asciz "/bin/bad"
child_path:        .asciz "/bin/child"
crash_path:        .asciz "/bin/crash"
text_fault_path:   .asciz "/bin/text-fault"
kernel_fault_path: .asciz "/bin/kernel-fault"
child_arg0:        .asciz "child"
child_arg1:        .asciz "ok"
child_env0:        .asciz "M5=1"
pass_message:      .ascii "M5 USER PASS\n"

    .section .data
    .align 3
child_argv: .dword child_arg0, child_arg1, 0
child_envp: .dword child_env0, 0
empty_vector: .dword 0
wait_status: .dword 0

    .section .bss
    .align 3
shared_word: .zero 8
