OUTPUT_ARCH(riscv)
ENTRY(_start)
SECTIONS
{
    . = 0x10000;
    .text : { *(.text .text.*) *(.rodata .rodata.*) }
    /DISCARD/ : { *(.comment) *(.riscv.attributes) }
}
