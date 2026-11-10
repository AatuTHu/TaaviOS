GLOBAL _start
EXTERN main
EXTERN __init_task

SECTION .text
BITS 64
_start:
    xor eax, eax
    xor edi, edi
    xor esi, esi
    xor edx, edx
    xor ebp, ebp
    and rsp, -16
    call __init_task
    cmp eax, -1
    je .init_failed

    call main
    mov eax, 1
    xor edi, edi
    int 0x80

.init_failed:
    mov eax, 1
    mov edi, -1
    int 0x80
