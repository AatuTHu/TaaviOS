[bits 64]

global bsp_stack_switch_and_call

bsp_stack_switch_and_call:
    mov rsp, rdi ;inc kstack_top
    xor rbp, rbp
    call rsi    ; rsi = kmain

.hang:
    cli
    hlt
    jmp .hang

global ap_stack_switch_and_call
ap_stack_switch_and_call:
    mov cr3, rdx      ; inc page dir phys addr
    mov rsp, rdi      ; inc kstack
    xor rbp, rbp
    call rsi          ; RSI = smp_ap_main

.hang:
    cli
    hlt
    jmp .hang
