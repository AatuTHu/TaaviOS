#include "tss.h"
#include "config.h"
#include "gdt.h"
#include "klog.h"

static struct tss_entry tss;

void tss_set_kernel_stack(uint32_t esp) {
    // klog("Setting tss.esp0 to 0x%x\n", esp);
    tss.esp0 = esp;
}

void tss_init(void) {
    tss = (struct tss_entry){0};

    tss.ss0  = SEG_KERNEL_DATA;
    tss.esp0 = 0;

    tss.iomap_base = sizeof(struct tss_entry);

    gdt_set_gate(5, (uint32_t)&tss, sizeof(struct tss_entry) - 1, 0x89, 0x00);

    klog("[TSS] TSS FLUSH BEGINS\n");
    tss_flush();
    klog("[TSS] TSS INITIALIZED SUCCESFULLY\n");
}