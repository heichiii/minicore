# MiniCore memory layout

M2 uses Sv39 with 4 KiB pages.  The kernel is loaded physically at
`0x80200000`, while only the small `_start` trampoline executes with address
translation disabled.  It installs a temporary root page table and jumps to
the high-half kernel.

| Region | Virtual address | Purpose |
| --- | --- | --- |
| Low half | unmapped after early boot | catches null and stale physical pointers |
| Direct map | `0xffffffc000000000 + PA` | RAM and selected MMIO |
| Kernel image | `0xffffffc080202000` onward | part of the direct map, with section permissions |

The final page table gives text `R-X`, rodata `R--`, and writable kernel/RAM
pages `RW-`.  The direct map is the kernel image mapping itself, so text has no
writable alias.  The first page reserved for the boot stack is deliberately
unmapped; the remaining three pages form the initial kernel stack.

UART, PLIC, VirtIO-MMIO, and QEMU's test-finisher page are mapped at their
direct-map virtual addresses.  No low identity mapping remains after the final
`satp` switch.

The physical allocator obtains RAM ranges from the DTB and excludes the
firmware prefix, kernel image (including bootstrap tables and stack), DTB,
the FDT memory reservation block, and `/reserved-memory`.  It is currently a
single-page free list.  Page-table pages come from the same allocator and are
returned recursively by `vm_destroy`; contiguous/order allocation is deferred
until a consumer needs it.

Sv39 leaf PTEs always set `A`; writable leaves also set `D`.  Writable-without-
readable mappings are rejected.  `map`, `unmap`, `protect`, and `query` reject
unaligned or non-canonical addresses.  M2 uses a full local `sfence.vma`; ASIDs
and remote shootdowns are deferred to SMP.
