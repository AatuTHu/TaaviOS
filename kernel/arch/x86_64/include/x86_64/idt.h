#ifndef IDT_H
#define IDT_H

#include <stdint.h>

#define IDT_GATE_INTERRUPT 0x8E

struct __attribute__((packed)) idt_entry {
    uint16_t offset_low;
    uint16_t selector;
    uint8_t ist;
    uint8_t type_attr;
    uint16_t offset_mid;
    uint32_t offset_high;
    uint32_t reserved;
};

struct __attribute__((packed)) idt_ptr {
    uint16_t limit;
    uint64_t base;
};

struct __attribute__((packed)) registers {
    uint64_t r15, r14, r13, r12, r11, r10, r9, r8;
    uint64_t rbp, rdi, rsi, rdx, rcx, rbx, rax;
    uint64_t int_no;
    uint64_t err_code;
    uint64_t rip, cs, eflags, rsp, ss;
};

extern uint64_t isr_stub_table[];
extern uint64_t irq_stub_table[];
extern void idt_flush(uint64_t idt_ptr_address);
void idt_load();
void idt_init();

#endif
