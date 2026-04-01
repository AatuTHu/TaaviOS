#include "vga.h"
#include "serial.h"
#include "klog.h"
#include "gdt.h"
#include "tss.h"
#include "idt.h"
#include "pit.h"
#include "pmm.h"
#include "mm.h"

#define PIT_FREQUENCY 1000

void init_drivers() {
    vga_init();
    serial_init();
}

void init_arch() {
    gdt_init();
    tss_init();
    idt_init();
}

void init_mm(uint32_t *mboot_info) {
    struct multiboot_info *mboot = (struct multiboot_info *)phys_to_virt((uint32_t)mboot_info);
    pmm_init(mboot);
    klog(1,"USED_PAGES %d\n", pmm_get_used_pages());
    klog(1,"FREE_PAGES %d\n", pmm_get_free_pages());
}

void kernel_main(uint32_t *mboot_info) {
    klog(1,"--INIT DRIVERS--\n");
    init_drivers();
    klog(1,"--INIT X86 SPECIFIC CODE\n");
    init_arch();
    klog(1,"--INIT MEMORY MANAGEMENT\n");
    init_mm(mboot_info);

    __asm__ __volatile__("sti");
    pit_init(PIT_FREQUENCY);

    uint32_t ticks = pit_get_ticks();
    klog(0,"Ticks from pit before sleep %d\n", ticks);
    pit_sleep_ms(5000);
    ticks = pit_get_ticks();
    klog(0,"Ticks after sleep: %d\n", ticks);
    
}