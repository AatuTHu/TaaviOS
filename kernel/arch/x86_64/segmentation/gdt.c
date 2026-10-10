#include "gdt.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include <stdint.h>

struct __attribute__((packed)) gdt_table {
    struct gdt_entry entries[5];
    struct gdt_tss_entry tss_entry;
};

static struct gdt_table gdt;

static struct tss_t tss;

static uint8_t df_stack[4096] __attribute__((aligned(16)));

static void gdt_set_entry(struct gdt_entry *e, uint8_t access, uint8_t flags) {
    e->base_high        = 0;
    e->base_mid         = 0;
    e->base_low         = 0;
    e->limit_low        = 0;
    e->access           = access;
    e->limit_high_flags = (flags << 4);
}

static void gdt_set_tss(struct gdt_tss_entry *e, uint64_t base, uint32_t limit) {
    e->low.limit_low        = (uint16_t)(limit & 0xFFFF);
    e->low.base_low         = (uint16_t)(base & 0xFFFF);
    e->low.base_mid         = (uint8_t)((base >> 16) & 0xFF);
    e->low.access           = 0x89;
    e->low.limit_high_flags = (uint8_t)(((limit >> 16) & 0x0F));
    e->low.base_high        = (uint8_t)((base >> 24) & 0xFF);
    e->base_upper           = (uint32_t)(base >> 32);
    e->reserved             = 0;
}

void gdt_init(void) {
    DEBUG_KERNEL("[GDT][INIT]: Preparing segmentation\n");
    memset(&gdt, 0, sizeof(gdt));
    memset(&tss, 0, sizeof(tss));

    gdt.entries[0] = (struct gdt_entry){0};
    gdt_set_entry(&gdt.entries[1], 0x9A, 0x2);
    gdt_set_entry(&gdt.entries[2], 0x92, 0x2);
    gdt_set_entry(&gdt.entries[3], SEG_64_USER_DATA, 0x2);
    gdt_set_entry(&gdt.entries[4], SEG_64_USER_CODE, 0x2);

    tss.rsp[0]                        = 0;
    uint64_t stack_top                = (uint64_t)df_stack + sizeof(df_stack);
    tss.ist[GDT_IST_DOUBLE_FAULT - 1] = stack_top;
    tss.iopb_offset                   = sizeof(struct tss_t);
    gdt_set_tss(&gdt.tss_entry, (uint64_t)&tss, sizeof(struct tss_t) - 1);

    struct gdtr gdtr_val;
    gdtr_val.limit = sizeof(struct gdt_table) - 1;
    gdtr_val.base  = (uint64_t)&gdt;

    gdt_flush((uint64_t)&gdtr_val);
    tss_flush();
    DEBUG_KERNEL("[GDT][INIT]: GDT and TSS set and flushed\n");
}
