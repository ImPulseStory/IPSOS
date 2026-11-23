ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)
SECTIONS
{
    . = 0x10000;

    .text : { *(.text .text.*) }
    .rodata : { *(.rodata .rodata.*) }
    .data : { *(.data .data.*) }
    .init_array : { KEEP(*(.init_array .init_array.*)) }
    .fini_array : { KEEP(*(.fini_array .fini_array.*)) }
    .eh_frame : { *(.eh_frame .eh_frame.*) }
    .gcc_except_table : { *(.gcc_except_table .gcc_except_table.*) }
    /* Keep kernel BSS above the legacy VGA/BIOS memory hole. */
    . = MAX(., 0x00100000);
    .bss :
    {
        __bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
        __bss_end = .;
    }

    . = ALIGN(4096);
    kernel_end = .;
}
