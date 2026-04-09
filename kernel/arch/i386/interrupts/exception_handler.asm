
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

; ── CPU-poikkeukset (ISR 0–31) ────────────────────────────────────────────────
; Intel määrittelee nämä. ERRCODE = CPU työntää virhekoodin pinoon automaattisesti.

ISR_NOERRCODE 0   ; #DE — Division Error (div/idiv nollalla tai tulokset liian iso)
ISR_NOERRCODE 1   ; #DB — Debug (yksittäisaskelus, breakpoint-rekisteri)
ISR_NOERRCODE 2   ;  NMI — Non-Maskable Interrupt (laitteistovirhe, ei voi estää)
ISR_NOERRCODE 3   ; #BP — Breakpoint (INT3-käsky, debuggeri käyttää)
ISR_NOERRCODE 4   ; #OF — Overflow (INTO-käsky, overflow-lippu asetettu)
ISR_NOERRCODE 5   ; #BR — BOUND Range Exceeded (BOUND-käsky ylittää rajat)
ISR_NOERRCODE 6   ; #UD — Invalid Opcode (tuntematon käsky tai virheellinen prefiksi)
ISR_NOERRCODE 7   ; #NM — Device Not Available (FPU-käsky ilman FPU:ta / TS-lippu)
ISR_ERRCODE    8  ; #DF — Double Fault (poikkeus poikkeuksen käsittelyn aikana)
ISR_NOERRCODE 9   ;       Coprocessor Segment Overrun (vanhentunut, 486+ ei käytä)
ISR_ERRCODE    10 ; #TS — Invalid TSS (tehtäväsegmentin virhe, esim. tilasiirto)
ISR_ERRCODE    11 ; #NP — Segment Not Present (segmentti merkitty ei-läsnäolevaksi)
ISR_ERRCODE    12 ; #SS — Stack Segment Fault (pino-overflow tai virheellinen SS)
ISR_ERRCODE    13 ; #GP — General Protection Fault (yleisin suojarikkomus, esim. ring)
ISR_ERRCODE    14 ; #PF — Page Fault (sivua ei löydy tai oikeusrikkomus, CR2 = osoite)
ISR_NOERRCODE 15  ;       Varattu Intelille — ei käytetä
ISR_NOERRCODE 16  ; #MF — x87 Floating-Point Exception (FPU-laskuvirhe)
ISR_ERRCODE    17 ; #AC — Alignment Check (muistiviittaus väärässä kohdistuksessa)
ISR_NOERRCODE 18  ; #MC — Machine Check (laitteistovika, malli-spesifi)
ISR_NOERRCODE 19  ; #XM — SIMD Floating-Point Exception (SSE-laskuvirhe)
ISR_NOERRCODE 20  ; #VE — Virtualization Exception (EPT-rikkomus, VMX)
ISR_NOERRCODE 21  ; #CP — Control Protection Exception (CET shadow stack -rikkomus)
ISR_NOERRCODE 22  ;       Varattu
ISR_NOERRCODE 23  ;       Varattu
ISR_NOERRCODE 24  ;       Varattu
ISR_NOERRCODE 25  ;       Varattu
ISR_NOERRCODE 26  ;       Varattu
ISR_NOERRCODE 27  ;       Varattu
ISR_NOERRCODE 28  ; #HV — Hypervisor Injection Exception (AMD SVM)
ISR_NOERRCODE 29  ; #VC — VMM Communication Exception (AMD SEV-ES)
ISR_ERRCODE    30 ; #SX — Security Exception (AMD SVM turvallisuusrikkomus)
ISR_NOERRCODE 31  ;       Varattu

; ── Laitteisto-IRQ:t (IRQ 0–15 → INT 32–47) ──────────────────────────────────
; PIC remap: Master 0x20–0x27 = INT 32–39, Slave 0x28–0x2F = INT 40–47

IRQ 0,  32  ; PIT  — Programmable Interval Timer (system tick)
IRQ 1,  33  ; PS/2 — Näppäimistö
IRQ 2,  34  ; PIC  — Slave-kaskadiline (Master pin 2 → Slave PIC, ei oikea laite)
IRQ 3,  35  ; UART — COM2 / COM4 sarjaportti
IRQ 4,  36  ; UART — COM1 / COM3 sarjaportti
IRQ 5,  37  ;        LPT2 / äänikortti (vaihtelee)
IRQ 6,  38  ;        Levykeasema (floppy)
IRQ 7,  39  ;        LPT1 / rinnakkaisportti (spurious IRQ mahdollinen)
IRQ 8,  40  ; RTC  — Real-Time Clock
IRQ 9,  41  ;        ACPI / vapaasti käytettävissä
IRQ 10, 42  ;        Vapaasti käytettävissä
IRQ 11, 43  ;        Vapaasti käytettävissä
IRQ 12, 44  ; PS/2 — Hiiri
IRQ 13, 45  ; FPU  — Coprocessor / FPU-virhe
IRQ 14, 46  ; IDE  — Ensisijainen ATA-väylä (primary disk)
IRQ 15, 47  ; IDE  — Toissijainen ATA-väylä (secondary disk)

isr_common:
    pusha
    mov eax, esp ;laitetaan eaxiin käyttäjän stääääck
    push eax
    call isr_handler
    add esp, 4
    popa
    add esp, 8
    iret

irq_common:
    pusha
    mov eax, esp ;laitetaan eaxiin käyttäjän stääääck
    push eax ;pusketaan eaxi irq handler funuun.
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
    popa
    add esp, 8
    iret