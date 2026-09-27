# MiniCore M6 syscall ABI

User programs use the RV64 `lp64` integer ABI. `ecall` receives the syscall
number in `a7`, up to six arguments in `a0`..`a5`, and returns a signed
result in `a0`. Negative results are errors.

| Number | Call | Arguments | Result |
|---:|---|---|---|
| 1 | `write` | `a0=fd`, `a1=buffer`, `a2=count` | bytes written or `-9` (`EBADF`) / `-14` (`EFAULT`) |
| 2 | `exit` | `a0=status` | does not return |
| 3 | `yield` | none | `0` |
| 4 | `getpid` | none | positive PID |
| 5 | `exec` | `a0=path`, `a1=argv`, `a2=envp` | returns only on error |
| 6 | `waitpid` | `a0=pid`, `a1=status`, `a2=options` | child PID or negative error |
| 7 | `fork` | none | child PID in parent, `0` in child |
| 8 | `openat` | dirfd, path, flags | lowest available fd |
| 9 | `close` | fd | 0 |
| 10 | `read` | fd, buffer, count | bytes read; 0 means EOF |
| 11 | `dup` | fd | lowest available fd sharing the description |
| 12 | `dup2` | fd, target | target; atomically replaces its old reference |
| 13 | `getdents` | fd, buffer, bytes | whole directory-entry bytes; 0 at end |
| 14 | `pipe` | pointer to two int32 values | 0; stores read fd, write fd |
| 15 | `chdir` | path | 0 |
| 16 | `getcwd` | buffer, size | byte count including NUL |
| 17 | `brk` | requested byte break (0 queries) | exact break or negative error |
| 18 | `mmap` | address, length, prot, flags, fd, offset | mapped address |
| 19 | `munmap` | address, length | 0 |
| 20 | `lseek` | fd, signed offset, whence | new byte offset |

Descriptors 0, 1 and 2 refer to the console through `file_ops`: stdin is
read-only; stdout/stderr are write-only. Unknown calls return `-38` (`ENOSYS`). `fork` shares
descriptor objects by reference and eagerly copies every user page. `exec`
keeps PID, parent/child relationships, and descriptors.

`exec` accepts an absolute or cwd-relative VFS path and null-terminated pointer vectors.
There may be at most eight arguments and eight environment strings; each path
or string, including its null byte, is limited to 64 bytes. A failed exec leaves
the old image intact. Supported files are little-endian RV64 static `ET_EXEC`
images without `PT_INTERP` or `PT_DYNAMIC`. Writable-executable segments are
rejected.

`waitpid` supports a positive child PID or `-1` for any child; `options` must
be zero. The raw signed 64-bit exit status is copied to `status` when non-null.
An invalid status pointer returns `-14` without consuming the zombie, so the
wait can be retried. No matching child returns `-10`.

User virtual addresses occupy the lower Sv39 half. ELF segments receive their
declared R/W/X permissions and must end at or below `0x3fb000`. Four RW, non-executable stack pages end at
`0x400000`, with an unmapped guard page below. The initial stack contains
`argc`, `argv[]`, a null pointer, `envp[]`, and a null pointer, and is aligned
to 16 bytes. Kernel mappings are present in the upper half without `PTE_U`.

`copy_from_user` and `copy_to_user` validate overflow and each page's U/R/W
permissions before copying through the RAM direct map. Consequently `SUM`
stays clear throughout M3 and no recoverable supervisor fault window is needed.
This is intentionally simple; a future demand-paged VM may replace it with a
short, explicitly recoverable `SUM` access window.

Each process currently owns exactly one scheduler thread. `exit` and fatal user
exceptions stop that thread. A reaper running on another kernel stack releases
runtime resources before publishing a small zombie record to the parent.

## Files, paths and errors

Constants and record layouts live in `src/abi.h`; this is a custom ABI, not
Linux binary compatibility. Each process has 32 fd slots. Separate opens have
independent offsets; dup and fork share offset, access mode and append flag.
Closing a slot drops exactly one reference. Exec preserves descriptors and cwd.

`AT_FDCWD=-100` selects cwd. Absolute paths ignore dirfd. Paths are limited to
255 bytes plus NUL, components to 63 bytes plus NUL. Repeated slashes, `.` and
`..` are supported; root cannot be escaped. A trailing slash requires a
directory. At most eight symlinks may be traversed. Relative link targets start
at the containing directory; absolute targets start at process root.
`O_NOFOLLOW` rejects a final link with `ELOOP`; a trailing slash requires
following the link to resolve a directory. Creating through a dangling link
returns `ENOENT`. No uid/gid permissions, hard links, unlink, mkdir, chroot or
runtime mount/unmount syscall is provided in M6.

The initramfs root is read-only. `/tmp` is a writable RAM filesystem (maximum
64 KiB per regular file, sparse holes read as zero); `/dev` contains `console`
and `null`. Mount-root `..` returns to the covered directory's parent.
`O_CREAT`, `O_EXCL`, `O_TRUNC`, `O_APPEND`, `O_DIRECTORY`, `O_NOFOLLOW` and the
three access modes are supported. `O_TRUNC` requires write access. File offsets
can seek past EOF; writes beyond the tmpfs limit return `EFBIG` or a prefix.

`getdents` writes fixed 80-byte records: uint64 inode, uint32 type, uint32 zero,
char name[64]. Names are NUL-terminated; unused bytes are zero. The offset is an
entry index shared by dup/fork. Entries are in insertion order, exclude `.` and
`..`, and expose symlinks rather than following them. The buffer must hold at
least one record. Seeking directories, pipes or devices returns `ESPIPE`.

Read/write validate the entire requested user range before side effects.
Invalid memory returns `EFAULT`, invalid descriptors/access modes `EBADF`.
Zero-byte read/write validates the descriptor/access mode but ignores the
buffer. Large reads/writes may return a prefix; stream reads return after one
backend read (at most 256 bytes). Pipe capacity is 512 bytes: empty reads block
while a writer exists; full writes block while a reader exists. Closing the
last writer yields EOF after buffered bytes drain; closing the last reader
wakes writers with `EPIPE` (no signal). M6 does not promise POSIX `PIPE_BUF`
atomicity for an entire syscall. Console input polls with scheduler sleeps;
`/dev/null` returns EOF on read and discards writes.

Common negative errors: `ENOENT=2`, `EBADF=9`, `ENOMEM=12`, `EFAULT=14`,
`EEXIST=17`, `ENOTDIR=20`, `EISDIR=21`, `EINVAL=22`, `EMFILE=24`, `EFBIG=27`,
`ESPIPE=29`, `EROFS=30`, `EPIPE=32`, `ENAMETOOLONG=36`, `ENOSYS=38`, `ELOOP=40`.
The M5 exec API still collapses missing/invalid ELF and loader failures to
`ENOENT`; invalid argument copies return `EFAULT`, argument allocation failure
returns `ENOMEM`.

## Memory regions

The heap begins at `0x800000` and can grow by at most 16 MiB. `brk` allocates
zeroed pages eagerly, returns a negative error on failure, and preserves the
old break on failure. Shrinking frees complete pages; bytes in a retained
partial page are not cleared or separately protected.

`mmap` uses `[0x10000000,0x20000000)` and accepts at most 16 MiB per call. Length
must be positive and is rounded up to 4096. Address and file offset must be
page-aligned. A nonfixed address is a hint; unavailable hints fall back to the
first free range. `MAP_FIXED_NOREPLACE` requires an address inside this window
and returns `EEXIST` on overlap. Replacing existing mappings is unsupported.

`MAP_PRIVATE` is required. Anonymous mappings additionally use `MAP_ANONYMOUS`,
fd=-1 and offset=0. File mappings require a readable regular file and eagerly
copy a private snapshot without advancing the file offset. EOF and holes are
zero-filled; closing or modifying the source later does not affect the mapping,
and mapped writes never update the source. There is no `MAP_SHARED`, demand
paging or writeback. `PROT_NONE` owns reserved zeroed pages without hardware
leaves; READ, READ|WRITE, EXEC, READ|EXEC are supported. WRITE without READ and
WRITE|EXEC are rejected. Unknown protection/mapping flags return `EINVAL`.

`munmap` requires aligned address and positive length; rounds length up and
accepts holes. It can remove complete mappings, trim either end, or split a
mapping in the middle. Any ELF/stack/heap intersection returns `EPERM=1` before
changing anything. A split allocates its new VMA before freeing pages, so
`ENOMEM` leaves the old mapping intact. Fork eagerly copies pages, VMA metadata
and the break; exec replaces all three. All rollback/unmap paths reclaim empty
private page-table branches and flush the local TLB.
