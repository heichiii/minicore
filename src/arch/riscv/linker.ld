OUTPUT_ARCH(riscv)
ENTRY(_start)

/* QEMU virt RAM begins at 0x80000000. OpenSBI occupies the first 2 MiB. */
KERNEL_BASE = 0x80200000;

SECTIONS
{
    . = KERNEL_BASE;
    __kernel_start = .;

    .text : ALIGN(4K)
    {
        __text_start = .;
        KEEP(*(.text.boot))
        *(.text .text.*)
        __text_end = .;
    }

    .rodata : ALIGN(4K)
    {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }

    .data : ALIGN(4K)
    {
        __data_start = .;
        *(.sdata .sdata.*)
        *(.data .data.*)
        __data_end = .;
    }

    /* Keep gp usable if the compiler emits small-data accesses. */
    __global_pointer$ = MIN(__data_start + 0x800,
                            MAX(__data_start + 0x800, __data_end - 0x800));

    .bss (NOLOAD) : ALIGN(4K)
    {
        __bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(16);
        __boot_stack_bottom = .;
        . += 16K;
        __boot_stack_top = .;
        __bss_end = .;
    }

    . = ALIGN(4K);
    __kernel_end = .;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame .eh_frame.*)
        *(.riscv.attributes)
    }
}

ASSERT((__boot_stack_top & 15) == 0, "boot stack must be 16-byte aligned")
