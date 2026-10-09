OUTPUT_FORMAT("elf64-x86-64")
OUTPUT_ARCH(i386:x86-64)

ENTRY(_start)

PHDRS
{
    text   PT_LOAD FLAGS(5);
    rodata PT_LOAD FLAGS(4);
    data   PT_LOAD FLAGS(6);
}

SECTIONS
{
    . = 0xffffffff80000000;

    kernel_start = .;

    . = ALIGN(0x1000);
    .text : {
        *(.text .text.*)
    } :text

    . = ALIGN(0x1000);
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    . = ALIGN(0x1000);
    .data : {
        *(.data .data.*)

        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :data

    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    . = ALIGN(0x1000);
    kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    }
}
