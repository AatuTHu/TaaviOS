#include "tss.h"
#include "cpu.h"

void tss_create(struct gdt_tss_entry *e, uint64_t base, uint32_t limit) {
    e->low.limit_low        = (uint16_t)(limit & 0xFFFF);
    e->low.base_low         = (uint16_t)(base & 0xFFFF);
    e->low.base_mid         = (uint8_t)((base >> 16) & 0xFF);
    e->low.access           = 0x89;
    e->low.limit_high_flags = (uint8_t)(((limit >> 16) & 0x0F));
    e->low.base_high        = (uint8_t)((base >> 24) & 0xFF);
    e->base_upper           = (uint32_t)(base >> 32);
    e->reserved             = 0;
}

void tss_set_kernel_stack(uint64_t stack_top) {
    cpu_current()->tss.rsp[0] = stack_top;
}
