[GLOBAL jump_to_usermode]

extern tss_set_kernel_stack;

jump_to_usermode:
    mov edx, [esp+4]
    mov ecx, [esp+8]
    mov eax, [esp+12]

    push eax
    call tss_set_kernel_stack

    mov eax, edx

    mov dx, 0x23
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    push 0x23
    push ecx
    push 0x202
    push 0x1B
    push eax

    iret