# MiniCore M4 syscall ABI

User programs use the RV64 `lp64` integer ABI. `ecall` receives the syscall
number in `a7`, up to three arguments in `a0`..`a2`, and returns a signed
result in `a0`. Negative results are errors. M3 defined `write` and `exit`; M4
adds cooperative `yield`:

| Number | Call | Arguments | Result |
|---:|---|---|---|
| 1 | `write` | `a0=fd`, `a1=buffer`, `a2=count` | bytes written or `-9` (`EBADF`) / `-14` (`EFAULT`) |
| 2 | `exit` | `a0=status` | does not return |
| 3 | `yield` | none | `0` |

Descriptors 1 and 2 refer to the console through a `file_ops` object. Descriptor
0 is currently closed. Unknown calls return `-38` (`ENOSYS`).

User virtual addresses occupy the lower Sv39 half. The embedded test image is
mapped RX at `0x10000`; two RW, non-executable stack pages end at `0x400000`.
Kernel mappings are present in the upper half but have no `PTE_U` permission.

`copy_from_user` and `copy_to_user` validate overflow and each page's U/R/W
permissions before copying through the RAM direct map. Consequently `SUM`
stays clear throughout M3 and no recoverable supervisor fault window is needed.
This is intentionally simple; a future demand-paged VM may replace it with a
short, explicitly recoverable `SUM` access window.
