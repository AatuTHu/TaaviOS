#include "gdt.h"
#include "config.h"
#include "cpu.h"
#include "klog.h"
#include "kstring.h"
#include "smp.h"
#include "tss.h"
#include <stdint.h>

struct __attribute__((packed)) gdt_table {
    struct gdt_entry entries[5];
    struct gdt_tss_entry tss_entry[ACPI_MAX_CPUS];
};

static struct gdt_table gdt;

static void gdt_set_entry(struct gdt_entry *e, uint8_t access, uint8_t flags) {
    e->base_high        = 0;
    e->base_mid         = 0;
    e->base_low         = 0;
    e->limit_low        = 0;
    e->access           = access;
    e->limit_high_flags = (flags << 4);
}

void gdt_init_core(struct core_t *c) {
    memset(&c->tss, 0, sizeof(c->tss));
    c->tss.ist[GDT_IST_DOUBLE_FAULT - 1] = (uint64_t)c->df_stack + sizeof(c->df_stack);
    c->tss.iopb_offset                   = sizeof(struct tss_t);
    tss_create(&gdt.tss_entry[c->processor_id], (uint64_t)&c->tss, sizeof(struct tss_t) - 1);
}

void gdt_flush_core(struct core_t *c) {
    struct gdtr gdtr_val;
    gdtr_val.limit = sizeof(struct gdt_table) - 1;
    gdtr_val.base  = (uint64_t)&gdt;
    gdt_flush((uint64_t)&gdtr_val);
    tss_flush(GDT_TSS + 16 * c->processor_id);
}

void gdt_init(void) {
    DEBUG_KERNEL("[GDT][INIT]: Preparing segmentation\n");
    memset(&gdt, 0, sizeof(gdt));

    gdt.entries[0] = (struct gdt_entry){0};
    gdt_set_entry(&gdt.entries[1], 0x9A, 0x2);
    gdt_set_entry(&gdt.entries[2], 0x92, 0x2);
    gdt_set_entry(&gdt.entries[3], SEG_64_USER_DATA, 0x2);
    gdt_set_entry(&gdt.entries[4], SEG_64_USER_CODE, 0x2);

    gdt_init_core(&cores[0]);
    gdt_flush_core(&cores[0]);
    DEBUG_KERNEL("[GDT][INIT]: GDT and TSS set and flushed\n");
}
