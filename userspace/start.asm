global _start
extern main
extern __init_task

section .text
_start:
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor ebp, ebp
    call __init_task
    cmp eax, -1
    je .init_failed

    call main
    mov eax, 1
    int 0x80

.init_failed:
    mov eax, 1
    mov ebx, -1
    int 0x80
    ;hlt
