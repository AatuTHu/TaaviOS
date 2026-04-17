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
#include "sched.h"
#include "usermode.h"
#include "stddef.h"
#include "elf.h"
#include "syscall.h"

static uint32_t mod_start = 0;
static uint32_t mod_end = 0;

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
}

void init_mm(uint32_t *mboot_info) {
    DEBUG("--INIT MEMORY MANAGEMENT--\n");
    struct multiboot_info *mboot = (struct multiboot_info *)phys_to_virt((uint32_t)mboot_info);
    pmm_init(mboot);
}

void check_for_modules (uint32_t mboot_info) {
    DEBUG("--CHECKING FOR MODULES--\n");
    struct multiboot_info *mbi = (struct multiboot_info *)phys_to_virt((uint32_t)mboot_info);



    if((mbi->mods_count) > 0) {
        DEBUG("Modules found\n");
        struct multiboot_mod *mod = (struct multiboot_mod *)phys_to_virt(mbi->mods_addr);
        mod_start = mod->mod_start;
        mod_end   = mod->mod_end;
        uint32_t start_page = mod_start / PAGE_SIZE;
        DEBUG("mod start_page: %d\n", start_page);
        uint32_t end_page   = (mod_end + PAGE_SIZE-1) / PAGE_SIZE;
        DEBUG("mod page_end: %d\n", end_page);
        for(uint32_t i = start_page; i < end_page; i++) {
            pmm_set_bit(i);
        }
    } else {
        DEBUG("MODULES NOT FOUND\n");
    }
}

void kernel_main(uint32_t *mboot_info) {

    set_debug_mode();
    set_log_level(2);

    init_serial_and_vga();

    init_mm(mboot_info);
    check_for_modules(mboot_info);

    init_arch();
    
    vmm_alloc(&kernel_page_dir, HEAP_START, HEAP_PAGES * PAGE_SIZE, PAGE_PRESENT | PAGE_RW);
    kmalloc_init((void*)HEAP_START, HEAP_PAGES * PAGE_SIZE);
    scheduler_init(); //this does nothing yet, but could set something later. Now only a klog inside it
    syscall_init();
    
    vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    klog("----------------------------------------------------------------------------\n");
    vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    klog("  |=====|    |====|    |====|                                               \n");
    klog(" |=|   |=|  |=|  |=|  |=|  |=|                                              \n");
    klog(" |=|       |=|    |=| |=----=|                                              \n");
    klog(" |=|       |=------=| |=|===|                                               \n");
    klog(" |=|   |=| |=|    |=| |=|  |==|                                             \n");
    klog("  |=====|  |=|    |=| |=|   |==|                                            \n");
    vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    klog("                               |====|     |====| |=======|  |=====|        \n");
    klog("                              |=|  |=|   |=|  |=|   |=|    |=|   |=|       \n");
    klog("                              |=----=|   |=|  |=|   |=|    |=|___          \n");
    klog("                              |=|===|    |=|  |=|   |=|          |=|       \n");
    klog("                              |=|  |==|  |=|  |=|   |=|    |=|   |=|       \n");
    klog("                              |=|   |==|  |====|    |=|     |=====|        \n");
    vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    klog("---------------------------------------------------------------------------\n");
    
    
    vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    klog("Carrots >");

    proc_t *first_proc = NULL;
    
    if(mod_start != 0) {
        page_directory_t *page_dir = paging_create_directory();
        mod_start = phys_to_virt(mod_start);
        DEBUG("mod_start: 0x%x\n", mod_start);
        uint32_t entry = elf_load((void*)mod_start, page_dir);
        if(entry == ET_NONE) {
            DEBUG("[KERNEL]: ELF load failed!\n");
        } else {
            DEBUG("[KERNEL]: ELF loaded.\n");
            DEBUG("[KERNEL]: Creating init process\n");
            first_proc = process_create(entry, "init", page_dir);
            scheduler_add(first_proc);
        }
    }

    pit_init(PIT_FREQUENCY);
    
    
    
    // Code here is unreachable
    __asm__ __volatile__("sti");
    if(first_proc != NULL) {
        DEBUG("[KERNEL]: ENTERING USERMODE HOLD ON TO YOUR HATS\n");
        _set_scheduler_on();
        vmm_switch(first_proc->page_dir);
        DEBUG("[KERNEL]: Jumping with EIP: %x, ESP: %x\n", first_proc->context.eip, first_proc->useresp);
        enter_usermode(first_proc->context.eip, first_proc->useresp, first_proc->kernel_stack);
    }
}