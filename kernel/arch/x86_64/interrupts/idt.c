#include "idt.h"
#include "apic.h"
#include "config.h"
#include "gdt.h"
#include "io.h"
#include "klog.h"
#include "kstring.h"
#include "sched.h"
#include "stdbool.h"
#include <stdint.h>

/**
 * Interrupt descriptor table
 * Design & Implementation:
 * @author: A.H, 2026
 */

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_pointer;
irq_callback_t irq_callbacks[16] = {0};

void irq_register_handler(int index, irq_callback_t cb) {
    DEBUG("[ISR] Registering callback %x\n", cb);
    irq_callbacks[index] = cb;
}

void isr_handler(struct registers *r) {

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
}

uint64_t irq_handler(struct registers *r) {
    if (r->int_no == LAPIC_TIMER_VECTOR) {
        lapic_tick_ticks(); // tick the ticks?
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

static void idt_set_gate(int vec, uint64_t handler, uint8_t type_attr, uint8_t ist) {
    idt[vec].offset_low  = (handler >> 0) & 0xFFFF;
    idt[vec].offset_mid  = (handler >> 16) & 0xFFFF;
    idt[vec].offset_high = (handler >> 32) & 0xFFFFFFFF;
    idt[vec].selector    = SEG_KERNEL_CODE;
    idt[vec].ist         = ist & 0x7;
    idt[vec].type_attr   = type_attr;
    idt[vec].reserved    = 0;
}

void idt_init(void) {
    memset(&idt, 0, sizeof(idt));

    DEBUG_KERNEL("[IDT]: Mapping 32 cpu exception stubs\n");
    for (int i = 0; i <= 31; i++) {
        uint8_t ist = 0;
        if (i == 8) {
            ist = GDT_IST_DOUBLE_FAULT;
        }
        idt_set_gate(i, (uint64_t)isr_stub_table[i], IDT_GATE_INTERRUPT, ist);
    }

    DEBUG_KERNEL("[IDT]: Mapping 16 hardware intterrupt stubs\n");
    for (int i = 32; i <= 47; i++) {
        idt_set_gate(i, irq_stub_table[i - 32], IDT_GATE_INTERRUPT, 0);
    }

    DEBUG_KERNEL("[IDT]: Mapping LAPIC timer interrupt vector (0x40)\n");
    idt_set_gate(LAPIC_TIMER_VECTOR, (uint64_t)&irq32, IDT_GATE_INTERRUPT, 0);

    DEBUG_KERNEL("[IDT]: Mapping lapic spurrious vector\n");
    idt_set_gate(255, (uint64_t)isr255, IDT_GATE_INTERRUPT, 0);

    idt_pointer.limit = sizeof(idt) - 1;
    idt_pointer.base  = (uint64_t)&idt;

    DEBUG_KERNEL("[IDT]: IDT FLUSH BEGINS\n");
    idt_flush((uint64_t)&idt_pointer);
    DEBUG_KERNEL("[IDT]: IDT INITIALIZED SUCCESFULLY\n");
}
