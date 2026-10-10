#ifndef GDT_H
#define GDT_H
#include <stdint.h>

#define GDT_IST_DOUBLE_FAULT 1

struct __attribute__((packed)) gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t base_mid;
    uint8_t access;
    uint8_t limit_high_flags;
    uint8_t base_high;
};

struct __attribute__((packed)) gdt_tss_entry {
    struct gdt_entry low;
    uint32_t base_upper;
    uint32_t reserved;
};

struct __attribute__((packed)) tss_t {
    uint32_t reserved0;
    uint64_t rsp[3];
    uint64_t reserved1;
    uint64_t ist[7];
    uint64_t reserved2;
    uint16_t reserved3;
    uint16_t iopb_offset;
};

struct __attribute__((packed)) gdtr {
    uint16_t limit;
    uint64_t base;
};

extern void gdt_flush(uint64_t gdtr);
extern void tss_flush();
void gdt_init(void);
#endif
