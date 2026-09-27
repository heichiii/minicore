# README

This is a mini os referencing rCore with a dead predecessor https://github.com/heichiii/mycore.

## Build and run

The kernel targets RV64 S-mode on QEMU `virt` and boots through OpenSBI:

```sh
make
make run
```

During boot the high-half kernel checks DTB discovery, deliberately handles
illegal-instruction, breakpoint, and load-access-fault exceptions, then
delivers three SBI timer interrupts while verifying every saved integer
register. It then installs its final Sv39 page table and tests physical-page
allocation, mapping, protection, unmapping, page-table reclamation, read-only
text, and the kernel-stack guard page. A successful boot exercises the M4 heap,
kernel threads, register-safe round-robin switching, timer preemption, sleeping,
and wait-queue synchronization.

M5 mounts an embedded read-only `newc` initramfs, loads `/init` as a static
ELF64 program, and builds its `argc/argv/envp` stack. `/init` exercises
`getpid`, eager-copy `fork`, transactional `exec`, `exit`, and blocking
`waitpid`, including failed exec, invalid status pointers, zombie retry,
orphan adoption, writable-memory independence, RX text, kernel isolation, and
process-local crashes. A successful run releases every user page, page table,
kernel stack, and descriptor reference, prints `M5 PASS`, and exits QEMU. See
[the M5 design](docs/m5.md).
`make test` runs this acceptance test with both 256 MiB and 32 MiB of RAM.

## Memory layout report

Build the kernel and generate a self-contained HTML visualization of its ELF
sections, load segments, RAM usage, and the QEMU `virt` physical address space:

```sh
make layout
```

Open `build/layout.html` in a browser. The report is generated from the current
ELF and a QEMU monitor snapshot rather than from hard-coded addresses. Alternate
RAM sizes can be inspected with, for example, `make layout QEMU_MEMORY=512M`.
