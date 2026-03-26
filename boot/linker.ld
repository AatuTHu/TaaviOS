ENTRY(boot_entry)
SECTIONS
{
    . = 0xC0100000;
    _kernel_start = . - 0xC0000000;

    .text : AT(0x100000)
    {
        *(.multiboot)
        *(.text)
        . = ALIGN(4096);
    }

    .rodata : AT(LOADADDR(.text) + SIZEOF(.text))
    {
        *(.rodata*)
        . = ALIGN(4096);
    }

    .data : AT(LOADADDR(.rodata) + SIZEOF(.rodata))
    {
        *(.data)
        . = ALIGN(4096);
    }

    .bss :
    {
        *(COMMON)
        *(.bss)
        . = ALIGN(4096);
    }

    _kernel_end = . - 0xC0000000;
}