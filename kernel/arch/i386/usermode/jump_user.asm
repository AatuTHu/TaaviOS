; Define GDT Selectors (Assuming standard 5-entry GDT)
%define USER_DATA_SEG 0x23 ; Index 4, RPL 3
%define USER_CODE_SEG 0x1B ; Index 3, RPL 3
%define EFLAGS_IF     0x202  ; Bit 9 (Interrupt Flag) set

[GLOBAL jump_to_usermode]
jump_to_usermode:
    ; Load arguments from the stack
    mov eax, [esp + 4]    ; Argument 1: Target EIP (User Entry Point)
    mov ecx, [esp + 8]    ; Argument 2: Target ESP (User Stack)

    ; Set up Data Segment Registers for Ring 3
    mov dx, USER_DATA_SEG
    mov ds, dx
    mov es, dx
    mov fs, dx
    mov gs, dx

    ; Manually construct the IRET stack frame
    ; [esp + 16] - SS (Stack Segment)
    ; [esp + 12] - ESP (Stack Pointer)
    ; [esp + 8]  - EFLAGS
    ; [esp + 4]  - CS (Code Segment)
    ; [esp + 0]  - EIP (Instruction Pointer)

    push USER_DATA_SEG    ; Push User SS
    push ecx              ; Push User ESP
    push EFLAGS_IF        ; Push EFLAGS with interrupts enabled
    push USER_CODE_SEG    ; Push User CS
    push eax              ; Push User EIP

    ; Execute the privilege level switch
    iret