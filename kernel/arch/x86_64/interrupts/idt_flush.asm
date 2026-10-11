[GLOBAL idt_flush]

idt_flush:
    lidt [rdi]
    ret
