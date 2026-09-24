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
