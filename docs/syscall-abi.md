# MiniCore M5 syscall ABI

User programs use the RV64 `lp64` integer ABI. `ecall` receives the syscall
number in `a7`, up to three arguments in `a0`..`a2`, and returns a signed
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

Descriptors 1 and 2 refer to the console through a `file_ops` object. Descriptor
0 is currently closed. Unknown calls return `-38` (`ENOSYS`). `fork` shares
descriptor objects by reference and eagerly copies every user page. `exec`
keeps PID, parent/child relationships, and descriptors.

`exec` accepts an absolute initramfs path and null-terminated pointer vectors.
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
declared R/W/X permissions. Four RW, non-executable stack pages end at
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
