[bits 32]
[GLOBAL switch_page_dir]


extern virt_to_phys;
KERNEL_VIRTUAL_BASE equ 0xC0000000

switch_page_dir:
    mov eax, dword [esp+4]; take in the page dir

    sub eax, KERNEL_VIRTUAL_BASE; lower the address

    mov cr3, eax

    ret
