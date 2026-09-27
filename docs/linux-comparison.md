# Linux comparison notes

## M2: physical memory and Sv39

MiniCore's free-list allocator corresponds only to Linux's lowest-level page
allocator role. Linux uses zones, a buddy allocator, per-CPU caches, NUMA
policies, page descriptors, and extensive accounting; MiniCore currently needs
only single 4 KiB pages and therefore stores a next pointer in each free page.

MiniCore's three-level page-table walk follows the Sv39 hardware format, like
Linux's RISC-V backend, but exposes a deliberately small `map/unmap/protect/query`
interface. It uses one kernel address space, no ASIDs, and a complete local TLB
flush. Linux supports multiple page sizes, per-process address spaces, ASIDs,
and SMP TLB shootdowns.

Both designs separate executable code, read-only data, writable data, and
guard regions. MiniCore also avoids a writable direct-map alias for its text,
but does not yet implement Linux's broader hardening, sparse-memory support,
or dynamic direct-map permission changes.

## M3: user access and file-backed writes

Linux normally copies user memory using architecture-specific fault-table
fixups and tightly scoped supervisor access. MiniCore M3 instead walks its small
Sv39 table, validates every page, and copies through the direct map while SUM
remains disabled. This avoids kernel faults now, at the cost of not supporting
demand paging during copies.

Like Linux, the syscall layer writes through a descriptor and operation table
rather than calling the UART directly. M3 has a fixed three-entry fd table and
one console file; reference counting, VFS objects, and concurrent access arrive
in later milestones.

## M4: allocation, scheduling, and waits

MiniCore's size-class allocator resembles the object-cache role of Linux slab
allocators, but has no per-CPU caches, constructors, debug metadata, or large
allocation path. Empty slabs are returned immediately to the physical-page
allocator.

Its thread context contains only RISC-V callee-saved integer registers; trap
frames preserve interrupted caller-saved state. The one-list round-robin
scheduler has no priorities, affinity, load balancing, or scheduling classes.
As in Linux, an idle thread runs when nothing else is runnable, timer ticks can
preempt execution, and sleeping threads leave the run queue rather than spin.

Wait queues use the same essential ordering as Linux wait primitives: enqueue
while holding the condition lock, release it only after becoming non-runnable,
and recheck conditions in a loop after wakeup. MiniCore currently relies on
local interrupt exclusion because it is single-hart; this is not a substitute
for global locking once SMP is enabled.

## M5: process resources, exec, and waiting

MiniCore now separates process-owned address-space resources from a thread's
kernel stack and saved execution state. These roles correspond to the address
space and task concepts in Linux, but MiniCore permits only one thread per
process and uses a page ledger instead of VMAs. Kernel threads explicitly use
the kernel page table rather than borrowing a user's active address space.
Reaping runs after switching away from the dead thread's stack and page table.

MiniCore now has monotonically allocated PIDs, parent/child lists, zombie
records, PID-1 orphan adoption, and blocking wait queues. Unlike Linux it has
no PID namespaces, process groups, signals, wait options, encoded wait status,
or multi-threaded exec rules. `fork` eagerly duplicates every user page rather
than using copy-on-write. `exec` prepares a complete static ELF image and
argument stack before replacing the old address space, following the same
prepare-then-commit principle as Linux while supporting far fewer ELF and ABI
features.

The read-only newc initramfs provides vnode lookup and refcounted file objects.
It is sufficient to decouple the ELF loader from storage, but lacks mounts,
cwd, links, permissions, and userspace open/read calls in the M5 snapshot.

## M6: VFS descriptions and memory regions

M6 separates fd slots, shared open descriptions, namespace nodes and boot
mounts. Like the corresponding Linux concepts, dup/fork share an open offset,
while independent opens do not. MiniCore intentionally omits unlink, inode
permissions, mount namespaces and vnode reference counting: namespace nodes
remain alive until shutdown. Pipes use wait queues and last-reference close
semantics; console and initramfs use the same kernel-buffer file interface.

VMA metadata is distinct from hardware page tables and physical-page ownership.
Unlike Linux's demand-paged file mappings, MiniCore eagerly copies private file
snapshots and anonymous pages. There is no MAP_SHARED, copy-on-write, writeback
or partial protection change. The fixed layout, mapping limits and custom
error/return conventions are specified in `syscall-abi.md`; they are teaching
choices, not Linux ABI compatibility claims.
