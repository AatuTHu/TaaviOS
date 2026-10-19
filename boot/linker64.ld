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

    _kernel_start = .;

    . = ALIGN(0x1000);
    _text_start = .;
    .text : {
        *(.text .text.*)
    } :text
    . = ALIGN(0x1000);
    _text_end = .;

    _rodata_start = .;
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata
    . = ALIGN(0x1000);
    _rodata_end = .;

    _data_start = .;
    .data : {
        *(.data .data.*)

        KEEP(*(.limine_requests_start))
        KEEP(*(.limine_requests))
        KEEP(*(.limine_requests_end))
    } :data

    _bss_start = .;
    .bss : {
        *(COMMON)
        *(.bss .bss.*)
    } :data
    . = ALIGN(0x1000);
    _bss_end = .;

    _data_end = .;
    _kernel_end = .;

    /DISCARD/ : {
        *(.eh_frame)
        *(.note .note.*)
    }
}
