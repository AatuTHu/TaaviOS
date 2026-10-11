[bits 64]

global stack_switch_and_call

stack_switch_and_call:
    mov rsp, rdi
    xor rbp, rbp
    call rsi

.hang:
    cli
    hlt
    jmp .hang
