#include "tss.h"
#include "gdt.h"

static struct tss_entry tss;

void tss_set_kernel_stack(uint32_t esp) {
    tss.esp0 = esp;
}

void tss_init(void) {
    tss = (struct tss_entry){0};
    
    tss.ss0 = SEG_KERNEL_DATA;
    tss.esp0 = 0; 
    
    tss.iomap_base = sizeof(struct tss_entry);
    
    gdt_set_gate(5, (uint32_t)&tss, sizeof(struct tss_entry) - 1, 0x89, 0x00);

    tss_flush();
}