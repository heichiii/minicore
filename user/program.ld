OUTPUT_ARCH(riscv)
ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5);
    data PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0x10000;
    .text : ALIGN(4K) { *(.text .text.*) *(.rodata .rodata.*) } :text
    . = ALIGN(4K);
    .data : ALIGN(4K) { *(.data .data.*) *(.sdata .sdata.*) } :data
    .bss : ALIGN(16) { *(.bss .bss.*) *(.sbss .sbss.*) *(COMMON) } :data
    /DISCARD/ : { *(.comment) *(.riscv.attributes) *(.eh_frame .eh_frame.*) }
}
