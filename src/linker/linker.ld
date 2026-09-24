OUTPUT_ARCH(riscv)
ENTRY(_start)

KERNEL_PHYS_BASE = 0x80200000;
KERNEL_DIRECT_BASE = 0xffffffc000000000;
KERNEL_VIRT_BASE = KERNEL_DIRECT_BASE + KERNEL_PHYS_BASE;

SECTIONS
{
    . = KERNEL_PHYS_BASE;
    __kernel_phys_start = .;
    .text.boot : ALIGN(4K) { KEEP(*(.text.boot)) }
    .bss.boot (NOLOAD) : ALIGN(4K) { *(.bss.boot) }

    . = KERNEL_VIRT_BASE + SIZEOF(.text.boot) + SIZEOF(.bss.boot);
    . = ALIGN(4K);
    __kernel_start = .;

    .text : AT(ADDR(.text) - KERNEL_DIRECT_BASE)
    {
        __text_start = .;
        *(.text .text.*)
        __text_end = .;
    }
    .rodata ALIGN(4K) : AT(ADDR(.rodata) - KERNEL_DIRECT_BASE)
    {
        __rodata_start = .;
        *(.rodata .rodata.*)
        __rodata_end = .;
    }
    .data ALIGN(4K) : AT(ADDR(.data) - KERNEL_DIRECT_BASE)
    {
        __data_start = .;
        *(.sdata .sdata.*)
        *(.data .data.*)
        __data_end = .;
    }

    __global_pointer$ = MIN(__data_start + 0x800,
                            MAX(__data_start + 0x800, __data_end - 0x800));

    .bss ALIGN(4K) (NOLOAD) : AT(ADDR(.bss) - KERNEL_DIRECT_BASE)
    {
        __bss_start = .;
        *(.sbss .sbss.*)
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4K);
        __boot_stack_guard = .;
        . += 4K;
        __boot_stack_bottom = .;
        . += 12K;
        __boot_stack_top = .;
        __bss_end = .;
    }

    . = ALIGN(4K);
    __kernel_end = .;
    __kernel_phys_end = __kernel_end - KERNEL_DIRECT_BASE;

    /DISCARD/ :
    {
        *(.comment)
        *(.eh_frame .eh_frame.*)
        *(.riscv.attributes)
    }
}

ASSERT((__boot_stack_top & 15) == 0, "boot stack must be 16-byte aligned")
ASSERT(SIZEOF(.text.boot) <= 4K, "boot trampoline is too large")
ASSERT(SIZEOF(.bss.boot) == 4K, "bootstrap root must occupy one page")
