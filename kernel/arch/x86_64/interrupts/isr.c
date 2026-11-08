#include "isr.h"
#include "apic.h"
#include "cpu.h"
#include "klog.h"
#include "sched.h"

irq_callback_t irq_callbacks[16] = {0};

void irq_register_handler(int index, irq_callback_t cb) {
    DEBUG("[ISR] Registering callback %x\n", cb);
    irq_callbacks[index] = cb;
}

uint64_t isr_handler(struct registers *r) {

    if (r->int_no == 129) {
        return scheduler_tick(r);
    }

    uint64_t cr2;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));

    int is_user = (r->cs & 0x3) == 3;

    ERROR("\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    ERROR("                     KERNEL PANIC                           \n");
    ERROR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    ERROR("REASON: ");
    if (r->int_no == 14) {
        ERROR("PAGE FAULT (Present: %s, Access: %s, Mode: %s)\n",
              (r->err_code & 0x1) ? "Yes" : "No",
              (r->err_code & 0x2) ? "Write" : "Read",
              (r->err_code & 0x4) ? "User" : "Kernel");
        ERROR("FAULTING ADDRESS: 0x%x\n", cr2);
    } else if (r->int_no == 13) {
        ERROR("GENERAL PROTECTION FAULT\n");
    } else {
        ERROR("EXCEPTION %d\n", r->int_no);
    }

    ERROR("LOCATION: %s mode at RIP 0x%x\n", is_user ? "USER" : "KERNEL", r->rip);
    ERROR("INTERRUPT: %d (Error Code: 0x%x)\n", r->int_no, r->err_code);

    ERROR("--- REGISTER DUMP ---\n");
    ERROR("RAX: 0x%x  RBX: 0x%x  RCX: 0x%x  RDX: 0x%x\n",
          r->rax, r->rbx, r->rcx, r->rdx);
    ERROR("RSI: 0x%x  RDI: 0x%x  RBP: 0x%x  RSP: 0x%x\n",
          r->rsi, r->rdi, r->rbp, r->rsp);
    ERROR("R8:  0x%x  R9:  0x%x  R10: 0x%x  R11: 0x%x\n",
          r->r8, r->r9, r->r10, r->r11);
    ERROR("R12: 0x%x  R13: 0x%x  R14: 0x%x  R15: 0x%x\n",
          r->r12, r->r13, r->r14, r->r15);
    ERROR("CS:  0x%x  EFLAGS: 0x%x  SS: 0x%x\n",
          r->cs, r->eflags, r->ss);

    if (is_user) {
        ERROR("USER STACK (RSP): 0x%x\n", r->rsp);
    }
    ERROR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    ERROR("System halted.\n");

    while (1) {
        __asm__ __volatile__("hlt");
    }

    return STATUS_OK;
}

uint64_t irq_handler(struct registers *r) {

    if (r->int_no == LAPIC_TIMER_VECTOR) {
        cpu_tick_ticks(); // tick the ticks?
        lapic_eoi();
        return scheduler_tick(r);
    }

    if (r->int_no >= 32 && r->int_no <= 47) {
        uint8_t irq = r->int_no - 32;
        if (irq_callbacks[irq] != 0) {
            irq_callbacks[irq]();
        }
        lapic_eoi();
        return (uint64_t)r;
    }

    DEBUG_KERNEL("[IRQ]: Unhandled interrupt vector: %u\n", r->int_no);
    lapic_eoi();
    return (uint64_t)r;
}
