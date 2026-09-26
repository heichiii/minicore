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
