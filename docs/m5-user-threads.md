# M5 foundation: scheduled user processes

This document describes the execution foundation added before the complete M5
process lifecycle. The finished initramfs, ELF, fork/exec/waitpid design and
current acceptance test are documented in [m5.md](m5.md).

Each user process owns one scheduler thread. The thread owns its kernel stack,
callee-saved kernel context, and the user trap frame stored at the top of that
stack. The process separately owns its address space, descriptor references,
PID, family links, and exit state. Kernel threads have no process and run with
the kernel page table.

With interrupts disabled, the scheduler selects the next thread, changes the
current pointer, activates that thread's process page table (or the kernel page
table), and switches kernel contexts. Every user page table borrows identical
upper-half kernel mappings, so kernel code and both kernel stacks remain valid
across the `satp` change. MiniCore still uses ASID zero and a complete local TLB
flush; this is a single-hart design.

On a U-mode trap, the assembly entry switches to the owning thread's kernel
stack and saves the real user `sp` from `sscratch`. It clears `sscratch` before
entering C so an S-mode trap is distinguishable. Returning to U-mode installs
the kernel stack top in `sscratch` and restores the saved user frame. Initial
launch and later trap returns use the same restore path.

An exiting user thread never frees its active stack or page table. It records
termination, becomes dead, and switches away. The scheduler later reaps the
thread from another stack and address space; the process layer then releases
user pages, private page tables, and descriptor references before exposing a
reapable zombie to the parent.
