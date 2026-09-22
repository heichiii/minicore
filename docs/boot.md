# MiniCore boot protocol

MiniCore initially targets RV64 QEMU `virt`. QEMU loads OpenSBI in M-mode and
loads `build/kernel.elf` at its linked address, `0x80200000`. OpenSBI prepares
the machine and enters `_start` in S-mode with address translation disabled.

At entry:

- `a0` contains the boot hart ID.
- `a1` contains the physical address of QEMU's flattened device tree.
- Other general-purpose registers and the stack must not be assumed usable.

`src/arch/riscv/head.S` preserves the two arguments, initializes `gp` and the boot
stack, clears `.bss`, and calls `kernel_main(hart_id, dtb)`. The initial stack
is 16 KiB and is part of `.bss`. Only the boot hart is supported in M0.

The linker keeps the kernel above OpenSBI's reserved area. M0 runs with the MMU
off, so linked addresses are physical addresses. The DTB must remain intact
until a later milestone parses and copies the required information.
QEMU `virt` UART and test-finisher code lives in `src/platform/qemu-virt.c`.

Build and test:

```sh
make
make test
```

`make run` prints the boot test and exits automatically. The M0 test prints its
boot arguments and section boundaries, checks `.bss` and the small C runtime,
and exits QEMU through its test finisher device. If execution hangs while you
are experimenting, leave QEMU with `Ctrl-a x`.

For source-level debugging, run `make debug`, then connect from another shell:

```sh
make gdb
(gdb) break _start
(gdb) continue
```
