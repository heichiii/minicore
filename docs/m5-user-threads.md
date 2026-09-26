# M5 step 1: scheduled user processes

This step makes user execution a scheduler-owned thread with a private address
space. It retains the embedded test image temporarily; initramfs, ELF loading,
PID/parent records, fork/exec/waitpid and refcounted VFS files remain later M5
work. A successful boot prints `M5 USER THREAD PASS`, not a full `M5 PASS`.

## Objects and ownership

`process` owns an `address_space` and a three-entry descriptor table. The current
console descriptors borrow a static console object and are cleared on teardown.
`address_space` owns the lower-half page-table pages and a dynamically sized
list of user pages (virtual address, physical page, permissions). Upper-half
kernel mappings are borrowed from the permanent kernel table. There is no
fixed 18-page allocation ledger any more. Destroying an address space is legal
only after all execution has left it.

Each user process has one owning scheduler thread. That thread owns its kernel
stack, callee-saved kernel context and user trap frame at the top of its stack.
`thread_create_user` transfers process ownership only on success; callers must
destroy an unpublished process after a failure. Kernel threads have no process.
Syscalls and user copies obtain the process from the scheduler's current thread.

The temporary `process_result` observer is for kernel acceptance tests only.
Its storage must live until reaping completes; it is not a userspace wait ABI.

## Switching and traps

With interrupts disabled, the scheduler selects the next thread, updates the
current pointer, activates that thread's process page table (or the kernel
page table), and switches kernel contexts. Every page table shares identical
upper-half kernel code and direct-map stack mappings, so both stacks remain
accessible across the `satp` change. ASID stays zero and activation flushes the
local TLB. This contract remains single-hart.

For a U-mode trap, the entry swaps onto the owning thread's kernel stack and
saves the original user `sp` from `sscratch` in the frame. For an S-mode trap,
it saves the original kernel `sp`. Both paths clear `sscratch` before entering
C. The return path sets `sscratch` to the kernel stack top only when returning
to U-mode. This fixes the old path that saved a kernel address as the user `sp`.

The initial user frame contains a clean integer context, entry PC, user stack
pointer and test argument. Its status permits U-mode return with interrupts,
with SUM and floating-point state disabled. Initial launch and subsequent trap
returns share the assembly restore path. The old global `user_kernel_sp`,
`user_enter` and `user_resume_kernel` round trip has been removed.

User exit or fault marks the thread dead and switches away without returning.
`scheduler_reap`, called by the test supervisor and idle thread, frees dead
threads' stacks, user pages, private page tables and process objects while
running on a different stack and page table. Ordinary user faults do not exit
QEMU. Physical-page free-list updates now disable and restore interrupts so
allocation remains safe under kernel preemption. Copies still validate page
permissions and use the direct map with SUM clear. Image loading uses `fence.i`;
the build explicitly enables Zifencei.

## Acceptance

`make test` checks M1-M4 and this step under both 256 MiB and 32 MiB RAM:

- Original cross-page copy, bad pointer, RX text and kernel-isolation tests now
  execute through scheduled user threads.
- Two users write different values at the same virtual address. Each exercises
  real stack frames, distinct register patterns, timer-only busy loops and
  explicit yield. Both must receive multiple U-mode timer interrupts.
- A third process executes an illegal instruction while the survivors run.
- The kernel supervisor verifies its own page table, null current process,
  zero `sscratch` and clear SUM on every wait iteration.
- Three complete batches restore the free-page baseline, including reclaimed
  heap slabs, process records, page tables and kernel stacks.
- A low-memory test holds free pages in an intrusive list and releases them one
  at a time. At each budget, failed creation or successful execution must return
  to the same free-page count. Both failure and success must be observed; all
  held pages are then released. It prints `M5 ALLOCATION ROLLBACK PASS`.

The user stack has two RW/NX pages with an unmapped page below. Thread kernel
stacks retain M4's one-page direct-map allocation; per-thread kernel guard-page
mappings are not introduced by this step. Kernel syscall handling keeps the
existing interrupt policy; user execution is timer-preemptible and yield can
switch from within a syscall.
