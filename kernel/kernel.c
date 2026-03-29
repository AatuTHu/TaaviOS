#include "vga.h"
#include "serial.h"
#include "klog.h"
#include "gdt.h"
#include "tss.h"
#include "idt.h"
#include "pit.h"

#define PIT_FREQUENCY 1000

void init_drivers() {
    vga_init();
    serial_init();
}

void init_x86() {
    gdt_init();
    tss_init();
    idt_init();
}

void kernel_main(uint32_t *mboot_info) {
    klog(1,"--INIT DRIVERS--\n");
    init_drivers();
    klog(1,"--INIT X86 SPECIFIC CODE\n");
    init_x86();

    __asm__ __volatile__("sti");
    pit_init(PIT_FREQUENCY);

    uint32_t ticks = pit_get_ticks();
    klog(0,"Ticks from pit before sleep %d\n", ticks);
    pit_sleep_ms(5000);
    ticks = pit_get_ticks();
    klog(0,"Ticks after sleep: %d\n", ticks);
    
}