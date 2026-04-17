global _start
extern main

section .text
_start:
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor ebp, ebp
    call main
    mov eax, 1
    int 0x80
    hlt