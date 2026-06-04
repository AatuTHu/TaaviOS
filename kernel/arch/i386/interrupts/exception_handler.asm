%macro ISR_NOERRCODE 1
global isr%1
isr%1:
    push dword 0
    push dword %1
    jmp isr_common
%endmacro

%macro ISR_ERRCODE 1
global isr%1
isr%1:
    push dword %1
    jmp isr_common
%endmacro

%macro IRQ 2
global irq%1
irq%1:
    push dword 0
    push dword %2
    jmp irq_common
%endmacro

extern isr_handler
extern irq_handler

ISR_NOERRCODE 0   ; #DE — Divide Error (div/idiv by zero or result too large)
ISR_NOERRCODE 1   ; #DB — Debug (single-step, breakpoint register)
ISR_NOERRCODE 2   ;  NMI — Non-Maskable Interrupt (hardware error, cannot be masked)
ISR_NOERRCODE 3   ; #BP — Breakpoint (INT3 instruction, used by debuggers)
ISR_NOERRCODE 4   ; #OF — Overflow (INTO instruction, overflow flag set)
ISR_NOERRCODE 5   ; #BR — BOUND Range Exceeded (BOUND instruction exceeds bounds)
ISR_NOERRCODE 6   ; #UD — Invalid Opcode (unknown instruction or invalid prefix)
ISR_NOERRCODE 7   ; #NM — Device Not Available (FPU instruction without FPU / TS flag)
ISR_ERRCODE    8  ; #DF — Double Fault (exception during exception handling)
ISR_NOERRCODE 9   ;       Coprocessor Segment Overrun (deprecated, 486+ does not use)
ISR_ERRCODE    10 ; #TS — Invalid TSS (task segment error, e.g., state switch)
ISR_ERRCODE    11 ; #NP — Segment Not Present (segment marked as not present)
ISR_ERRCODE    12 ; #SS — Stack Segment Fault (stack overflow or invalid SS)
ISR_ERRCODE    13 ; #GP — General Protection Fault (most common protection violation, e.g., ring)
ISR_ERRCODE    14 ; #PF — Page Fault (page not found or privilege violation, CR2 = address)
ISR_NOERRCODE 15  ;       Reserved by Intel — not used
ISR_NOERRCODE 16  ; #MF — x87 Floating-Point Exception (FPU math error)
ISR_ERRCODE    17 ; #AC — Alignment Check (misaligned memory reference)
ISR_NOERRCODE 18  ; #MC — Machine Check (hardware fault, model-specific)
ISR_NOERRCODE 19  ; #XM — SIMD Floating-Point Exception (SSE math error)
ISR_NOERRCODE 20  ; #VE — Virtualization Exception (EPT violation, VMX)
ISR_NOERRCODE 21  ; #CP — Control Protection Exception (CET shadow stack violation)
ISR_NOERRCODE 22  ;       Reserved
ISR_NOERRCODE 23  ;       Reserved
ISR_NOERRCODE 24  ;       Reserved
ISR_NOERRCODE 25  ;       Reserved
ISR_NOERRCODE 26  ;       Reserved
ISR_NOERRCODE 27  ;       Reserved
ISR_NOERRCODE 28  ; #HV — Hypervisor Injection Exception (AMD SVM)
ISR_NOERRCODE 29  ; #VC — VMM Communication Exception (AMD SEV-ES)
ISR_ERRCODE    30 ; #SX — Security Exception (AMD SVM security violation)
ISR_NOERRCODE 31  ;       Reserved
ISR_NOERRCODE 129 ;       YIELD FORCE VECTOR

; ── Hardware IRQs (IRQ 0–15 → INT 32–47) ──────────────────────────────────
; PIC remap: Master 0x20–0x27 = INT 32–39, Slave 0x28–0x2F = INT 40–47

IRQ 0,  32  ; PIT  — Programmable Interval Timer (system tick)
IRQ 1,  33  ; PS/2 — Keyboard
IRQ 2,  34  ; PIC  — Slave cascade line (Master pin 2 → Slave PIC, not a real device)
IRQ 3,  35  ; UART — COM2 / COM4 serial port
IRQ 4,  36  ; UART — COM1 / COM3 serial port
IRQ 5,  37  ;        LPT2 / sound card (varies)
IRQ 6,  38  ;        Floppy disk drive
IRQ 7,  39  ;        LPT1 / parallel port (spurious IRQ possible)
IRQ 8,  40  ; RTC  — Real-Time Clock
IRQ 9,  41  ;        ACPI / available for use
IRQ 10, 42  ;        Available for use
IRQ 11, 43  ;        Available for use
IRQ 12, 44  ; PS/2 — Mouse
IRQ 13, 45  ; FPU  — Coprocessor / FPU error
IRQ 14, 46  ; IDE  — Primary ATA channel (primary disk)
IRQ 15, 47  ; IDE  — Secondary ATA channel (secondary disk)

isr_common:
    pusha
    mov eax, esp ; Save user stack pointer to eax
    push eax ; this pushes eax?
    call isr_handler
    add esp, 4
    popa
    add esp, 8
    iret

irq_common:
    pusha
    mov eax, esp ; Save user stack pointer to eax
    push eax ; Push eax into the irq handler function
    call irq_handler 
    add esp, 4
    popa
    add esp, 8
    iret

global isr_stub_table
isr_stub_table:
%assign i 0
%rep 32
dd isr%+i
%assign i i+1
%endrep

global irq_stub_table
irq_stub_table:
%assign i 0
%rep 16
dd irq%+i
%assign i i+1
%endrep

extern syscall_dispatch

global syscall_handler
syscall_handler:
    push dword 0
    push dword 0
    pusha
    mov eax, esp
    push eax
    call syscall_dispatch
    add esp, 4
    mov [esp + 28], eax
    popa
    add esp, 8
    iret