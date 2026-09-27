    .section .text
    .globl _start
_start:
    la t0, _start
    sd zero, 0(t0)
1:  j 1b
