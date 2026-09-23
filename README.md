# README

This is a mini os referencing rCore with a dead predecessor https://github.com/heichiii/mycore.

## Memory layout report

Build the kernel and generate a self-contained HTML visualization of its ELF
sections, load segments, RAM usage, and the QEMU `virt` physical address space:

```sh
make layout
```

Open `build/layout.html` in a browser. The report is generated from the current
ELF and a QEMU monitor snapshot rather than from hard-coded addresses. Alternate
RAM sizes can be inspected with, for example, `make layout QEMU_MEMORY=512M`.
