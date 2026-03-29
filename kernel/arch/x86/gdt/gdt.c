#include "gdt.h"

struct gdt_entry gdt[GDT_ENTRIES];
struct gdt_ptr gdt_pointer;

void gdt_set_gate(int num, uint32_t base, uint32_t limit, uint8_t access, uint8_t flags) {
    gdt[num].base_low = base & 0xFFFF;
    gdt[num].base_middle =(base >> 16) & 0xFF;
    gdt[num].base_high = (base >> 24) & 0xFF;
    gdt[num].limit_low = limit & 0xFFFF;
    gdt[num].granularity =((flags & 0x0F) << 4) | ((limit >> 16) & 0x0F);
    gdt[num].access = access;
}

void gdt_init(void) {
    gdt_pointer.limit = sizeof(struct gdt_entry) * GDT_ENTRIES - 1;
    gdt_pointer.base = (uint32_t)&gdt;

    gdt_set_gate(0, 0x00000000, 0x00000, 0x00, 0x0);
    gdt_set_gate(1, 0x00000000, 0xFFFFF, 0x9A, 0xC);
    gdt_set_gate(2, 0x00000000, 0xFFFFF, 0x92, 0xC);
    gdt_set_gate(3, 0x00000000, 0xFFFFF, 0xFA, 0xC);
    gdt_set_gate(4, 0x00000000, 0xFFFFF, 0xF2, 0xC);
    
    gdt_flush((uint32_t)&gdt_pointer);
}