#include "isr.h"
#include "io.h"
#include "config.h"
#include "klog.h"
#include "sched.h"


irq_callback_t irq_callbacks[16] = {0};

void irq_register_handler(int index, irq_callback_t cb) {
    DEBUG("[ISR] Registering callback %x\n", cb);
    irq_callbacks[index] = cb; 
}
  
void isr_handler(struct registers *r) {
    uint32_t cr2;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));
        ERROR("KERNEL PANIC - ISR NUMBER %d\n PAGE FAULT ADDRESS: %x | ERROR_CODE: %x\n\n\
            REGISTERS EIP: %x\n ESP: %x\n ESI: %x\n EAX: %x\n EBP: %x\n EBX: %x\n ECX: %x\n EDI: %x\n EDX: %x\n EFLAGS: %x\n CS: %x\n",
            r->int_no, cr2, r->err_code, r->eip, r->esp, r->esi, r->eax, r->ebp, r->ebx, r->ecx, r->edi, r->edx, r->eflags, r->cs);
    while(1) { __asm__ __volatile__("hlt"); }
}

void irq_handler(struct registers *r) {
    int irq_index = r->int_no - 32;

    if (irq_index < 0 || irq_index > 15) {
        outb(0x20, 0x20);
        return;
    }

    if (irq_index == 0) {
       scheduler_tick(r);
    }

    if (irq_callbacks[irq_index] != 0) {
        irq_callbacks[irq_index]();
    }

    if (r->int_no >= 40) outb(0xA0, 0x20);
    outb(0x20, 0x20);
}