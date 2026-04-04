#include "vga.h"
#include "serial.h"
#include "klog.h"
#include "gdt.h"
#include "tss.h"
#include "idt.h"
#include "pit.h"
#include "pmm.h"
#include "mm.h"
#include "paging.h"
#include "vmm.h"
#include "kmalloc.h"

void init_serial_and_vga() {
    vga_init();
    serial_init();
}

void init_arch() {
    gdt_init();
    tss_init();
    idt_init();
    paging_init();

    klog("USED_PAGES %d\n", pmm_get_used_pages());
    klog("FREE_PAGES %d\n", pmm_get_free_pages());
}

void init_mm(uint32_t *mboot_info) {
    struct multiboot_info *mboot = (struct multiboot_info *)phys_to_virt((uint32_t)mboot_info);
    pmm_init(mboot);
    
    klog("USED_PAGES %d\n", pmm_get_used_pages());
    klog("FREE_PAGES %d\n", pmm_get_free_pages());
}

void kernel_main(uint32_t *mboot_info) {
    
    set_debug_mode(1);
    init_serial_and_vga();
    klog("--INIT SERIAL & VGA--\n");
    klog("--INIT MEMORY MANAGEMENT--\n");
    init_mm(mboot_info);
    klog("--INIT X86 SPECIFIC CODE--\n");
    init_arch();

    vmm_alloc(&kernel_page_dir, HEAP_START, HEAP_PAGES * PAGE_SIZE, PAGE_PRESENT | PAGE_RW);
    kmalloc_init((void*)HEAP_START, HEAP_PAGES * PAGE_SIZE);
    
    void *a = kmalloc(64);
    void *b = kmalloc(128);
    klog("a: %x, b: %x\n", a, b);
    kfree(a);
    void *c = kmalloc(32);
    klog("c: %x\n", c);

    __asm__ __volatile__("sti");
    pit_init(PIT_FREQUENCY);
    
    set_debug_mode(2);
    uint32_t ticks = pit_get_ticks();
    klog("Ticks from pit before sleep %d\n", ticks);
    pit_sleep_ms(5000);
    ticks = pit_get_ticks();
    klog("Ticks after sleep: %d\n", ticks);
    
}