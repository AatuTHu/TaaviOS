#ifndef TSS_H
#define TSS_H

#include "gdt.h"
#include <stdint.h>

struct __attribute__((packed)) tss_t {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
};

struct __attribute__((packed)) gdt_tss_entry {
    struct gdt_entry low;
    uint32_t base_upper;
    uint32_t reserved;
};

extern void tss_flush(uint16_t selector);
void tss_set_kernel_stack(uint64_t stack_top);
void tss_create(struct gdt_tss_entry *e, uint64_t base, uint32_t limit);

#endif
