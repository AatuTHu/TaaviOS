#include "idt.h"
#include "io.h"
#include "config.h"
#include "klog.h"

struct idt_entry idt[256];
struct idt_ptr idt_pointer;
void syscall_dispatch(struct registers *r) { (void)r; }


void pic_remap() {
    outb(PIC1_COMMAND, 0x11);
    outb(PIC2_COMMAND, 0x11);
    
    outb(PIC1_DATA, 0x20);
    outb(PIC2_DATA, 0x28);
    
    outb(PIC1_DATA, 0x04);
    outb(PIC2_DATA, 0x02);
    
    outb(PIC1_DATA, 0x01);
    outb(PIC2_DATA, 0x01);
    
    outb(PIC1_DATA, 0x0);
    outb(PIC2_DATA, 0x0);
    DEBUG("Remapping complete\n");
}

void idt_set_gate(int n, uint32_t handler, uint8_t dpl) {
    idt[n].base_low = handler & 0xFFFF;
    idt[n].base_high = (handler >> 16) & 0xFFFF;
    idt[n].selector = 0x08;
    idt[n].zero = 0;
    idt[n].flags = 0x8E | (dpl << 5);
}



void idt_init() {
    idt_pointer.limit = sizeof(idt) - 1;
    idt_pointer.base = (uint32_t)&idt;

    DEBUG("Remapping PIC\n");
    pic_remap();

    DEBUG("Mapping 32 cpu exception stubs\n");
    for(int i = 0; i <= 31; i++) {
        idt_set_gate(i, (uint32_t)isr_stub_table[i], 0);
    }

    DEBUG("Mapping 16 hardware intterrupt stubs\n");
    for(int i = 32; i <= 47; i++) {
        idt_set_gate(i, (uint32_t)irq_stub_table[i-32], 0);
    }

    //DEBUG("Setting system call gate to 0x80\n");
    //idt_set_gate(0x80, (uint32_t)syscall_handler, 3);

    DEBUG("IDT FLUSH BEGINS\n");
    idt_flush((uint32_t)&idt_pointer);
    DEBUG("IDT INITIALIZED SUCCESFULLY\n");
}