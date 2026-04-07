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
#include "proc.h"

void proc1() {
    klog("PROC1 RULES!\n");
}

void init_serial_and_vga() {
    vga_init();
    serial_init();
    DEBUG("--INIT SERIAL & VGA--\n");
}

void init_arch() {
    DEBUG("--INIT ARCH--\n");
    gdt_init();
    tss_init();
    idt_init();
    paging_init();

    //klog("USED_PAGES %d\n", pmm_get_used_pages());
    //klog("FREE_PAGES %d\n", pmm_get_free_pages());
}

void init_mm(uint32_t *mboot_info) {
    DEBUG("--INIT MEMORY MANAGEMENT--\n");
    struct multiboot_info *mboot = (struct multiboot_info *)phys_to_virt((uint32_t)mboot_info);
    pmm_init(mboot);
    
    //klog("USED_PAGES %d\n", pmm_get_used_pages());
    //klog("FREE_PAGES %d\n", pmm_get_free_pages());
}

void kernel_main(uint32_t *mboot_info) {
    
    set_debug_mode();
    set_log_level(2);

    init_serial_and_vga();

    init_mm(mboot_info);

    init_arch();

    vmm_alloc(&kernel_page_dir, HEAP_START, HEAP_PAGES * PAGE_SIZE, PAGE_PRESENT | PAGE_RW);
    kmalloc_init((void*)HEAP_START, HEAP_PAGES * PAGE_SIZE);

    vga_set_color(VGA_COLOR_MAGENTA, VGA_COLOR_BLACK);
    klog("  |=====|    |====|    |====|    |===|     |====|  |=======|  |=====|         \n");
    klog(" |=     =|  |=|  |=|  |=    =|  |=    =|  |=    =|    |=|    |=|   |=|        \n");
    klog(" |=        |=|    |=| |=----=|  |=----=|  |=    =|    |=|    |=|___           \n");
    klog(" |=        |=------=| |=|===|   |=|===|   |=    =|    |=|          |=|        \n");
    klog(" |=     =| |=|    |=| |=  |==|  |=  |==|  |=    =|    |=|    |=|   |=|        \n");
    klog("  |=====|  |=|    |=| |=   |==| |=   |==|  |====|     |=|     |=====|         \n");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    
    process_create((uint32_t)proc1, "proc1");
    
    pit_init(PIT_FREQUENCY);
    __asm__ __volatile__("sti");
    
}