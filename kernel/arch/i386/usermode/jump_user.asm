[GLOBAL jump_to_usermode]
jump_to_usermode:
    mov eax, [esp+4]
    mov ecx, [esp+8]

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