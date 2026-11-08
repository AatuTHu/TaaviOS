#include "idt.h"
#include "apic.h"
#include "config.h"
#include "gdt.h"
#include "io.h"
#include "isr.h"
#include "klog.h"
#include "kstring.h"
#include "stdbool.h"
#include <stdint.h>

/**
 * Interrupt descriptor table
 * Design & Implementation:
 * @author: A.H, 2026
 */

static struct idt_ptr idt_pointer;
static struct idt_entry idt[IDT_ENTRIES];

/**
 * PIC_REMAP_AND_MASK - Used to silence older PIC.
 *
 * Description:
 * This function moves the older pics interrupts vectors to safer zones
 * and then disables the pic. First we send to pic1/2 cmd port info that tells to expect
 * initialization params. Then we send to pic1/2 the addr from where the interrupt vectors
 * start (0x20/0x28). After that we connect master and salve to each other. Then
 * we set them to the ancient 8086 arch. And finally we mask the ports. After that
 * we check if the all went as planned and then return status according to check
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
static int pic_remap_and_mask(void) {

    outb(PIC1_CMD, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();
    outb(PIC2_CMD, PIC_ICW1_INIT | PIC_ICW1_ICW4);
    io_wait();

    outb(PIC1_DATA, PIC1_VECTOR_BASE);
    io_wait();
    outb(PIC2_DATA, PIC2_VECTOR_BASE);
    io_wait();

    outb(PIC1_DATA, 0x04);
    io_wait();
    outb(PIC2_DATA, 0x02);
    io_wait();

    outb(PIC1_DATA, PIC_ICW4_8086);
    io_wait();
    outb(PIC2_DATA, PIC_ICW4_8086);
    io_wait();

    outb(PIC1_DATA, 0xFF);
    io_wait();
    outb(PIC2_DATA, 0xFF);
    io_wait();

    if (inb(PIC1_DATA) == 0xFF && inb(PIC2_DATA) == 0xFF) {
        return STATUS_OK;
    } else {
        return STATUS_ERROR;
    }
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

void idt_load() {
    idt_flush((uint64_t)&idt_pointer);
}

void idt_init() {
    memset(&idt, 0, sizeof(idt));

    if (pic_remap_and_mask() == STATUS_ERROR) {
        ERROR("[APIC][INIT]: Failed to remap and mask PIC\n");
        return;
    }

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

    DEBUG_KERNEL("[IDT]: Setting task yield gate to 0x81\n");
    idt_set_gate(0x81, (uint64_t)isr129, IDT_GATE_INTERRUPT, 0);

    idt_pointer.limit = sizeof(idt) - 1;
    idt_pointer.base  = (uint64_t)&idt;

    DEBUG_KERNEL("[IDT]: IDT FLUSH BEGINS\n");
    idt_load();
    DEBUG_KERNEL("[IDT]: IDT INITIALIZED SUCCESFULLY\n");
}
