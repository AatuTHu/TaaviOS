#ifndef GDT_H
#define GDT_H
#include <stdint.h>

#define GDT_IST_DOUBLE_FAULT 1

struct core_t;

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
};

struct __attribute__((packed)) gdtr {
    uint16_t limit;
    uint64_t base;
};

extern void gdt_flush(uint64_t gdtr);
void gdt_init_core(struct core_t *c);
void gdt_flush_core(struct core_t *c);
void gdt_init(void);
#endif
