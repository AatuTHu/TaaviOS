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
#include "keyboard.h"
#include "ata.h"
#include "io.h"
#include "mbr.h"
#include "fat32.h"

#define MAX_MODS 10
uint32_t module_starts[MAX_MODS];
uint32_t module_ends[MAX_MODS];
uint32_t total_mods = 0;

void init_serial_and_vga() {
    vga_init();
    serial_init();
    DEBUG("[KERNEL]: --INIT SERIAL & VGA--\n");
}

void init_arch() {
    DEBUG("[KERNEL]: --INIT ARCH--\n");
    gdt_init();
    tss_init();
    idt_init();
    paging_init();
}

void init_mm(uint32_t *mboot_info) {
    DEBUG("[KERNEL]: --INIT MEMORY MANAGEMENT--\n");
    struct multiboot_info *mboot = (struct multiboot_info *)phys_to_virt((uint32_t)mboot_info);
    pmm_init(mboot);
}

void check_for_modules (uint32_t *mboot_info) {
    DEBUG("[KERNEL]: --CHECKING FOR MODULES--\n");
    struct multiboot_info *mbi = (struct multiboot_info *)phys_to_virt((uint32_t)mboot_info);
    
    if (mbi->mods_count > 0) {
        struct multiboot_mod *mods = (struct multiboot_mod *)phys_to_virt(mbi->mods_addr);
        total_mods = mbi->mods_count;

        for (uint32_t i = 0; i < total_mods && i < MAX_MODS; i++) {
            module_starts[i] = mods[i].mod_start;
            module_ends[i] = mods[i].mod_end;

            uint32_t start_page = mods[i].mod_start / PAGE_SIZE;
            uint32_t end_page = (mods[i].mod_end + PAGE_SIZE - 1) / PAGE_SIZE;
            for (uint32_t p = start_page; p < end_page; p++) {
                pmm_set_bit(p);
            }
        }
    } else {
        DEBUG("[KERNEL]: MODULES NOT FOUND\n");
    }
}

void init_drivers() {
    keyboard_init();
    ata_init();
    DEBUG("[ATA]: Status at init: %x\n", inb(ATA_STATUS));
    DEBUG("[ATA]: Alt status: %x\n", inb(ATA_ALT_STATUS));

    uint32_t fat32_lba = 0;
    uint32_t fat32_sectors = 0;

    if (mbr_find_fat32(&fat32_lba, &fat32_sectors) != 0) {
        DEBUG("[MBR]: FAT32 partition not found!\n");
    }

    fat32_init(fat32_lba);

    fat32_list_dir(f32_fs.root_cluster);
    /*uint32_t file_cluster = 0;
    uint32_t file_size = 0;
    
    uint8_t data[] = "Hello from Carrots OS!";
    uint32_t size = sizeof(data) -1;
    
    uint32_t cluster = fat32_write_file(data, size);
    fat32_create_dirent(f32_fs.root_cluster, "helli.txt", cluster, size);


    if (fat32_find_file(f32_fs.root_cluster, "HELLI   ", "TXT", &file_cluster, &file_size) == 0) {
        DEBUG("[FAT32]: Found file! cluster: %d size: %d\n", file_cluster, file_size);
        uint8_t file_buf[512];
        fat32_read_file(file_cluster, file_size, file_buf);
        file_buf[file_size] = '\0';
        DEBUG("[FAT32]: contents: %s\n", file_buf);
    } else {
        DEBUG("[FAT32]: File not found!\n");
    }*/
}

void kernel_main(uint32_t *mboot_info) {

    set_print_level(2);

    init_serial_and_vga();

    init_mm(mboot_info);
    check_for_modules(mboot_info);
    
    
    init_arch();
    init_drivers();
    
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

    proc_t *first_proc = NULL;

    if(total_mods > 0) {
        page_directory_t *init_pd = paging_create_directory();
        uint32_t init_phys = module_starts[0];
        uint32_t entry = elf_load((void*)phys_to_virt(init_phys), init_pd);
        if(entry == ET_NONE) {
            DEBUG("[KERNEL]: ELF load failed!\n");
        } else {
            DEBUG("[KERNEL]: ELF loaded.\n");
            DEBUG("[KERNEL]: Creating init process\n");
            first_proc = process_create(entry, "init", init_pd, USER_PROCESS);
            first_proc->priority = PRIORITY_LOW;
            scheduler_add(first_proc);
        }
    }

    if(total_mods > 1) {
        page_directory_t *shell_pd = paging_create_directory();
        page_directory_t *shell_pd2 = paging_create_directory();
        uint32_t shell_phys = module_starts[1];
        uint32_t entry = elf_load((void*)phys_to_virt(shell_phys), shell_pd); 
        uint32_t entry2 = elf_load((void*)phys_to_virt(shell_phys), shell_pd2);    
        if (entry != ET_NONE) {
            process_create(entry, "shell", shell_pd, USER_PROCESS);
            process_create(entry, "shell2", shell_pd2, USER_PROCESS);
        }
    }

    pit_init(PIT_FREQUENCY);
    
    __asm__ __volatile__("sti");
    
    if(first_proc != NULL) {
        DEBUG("[KERNEL]: ENTERING USERMODE HOLD ON TO YOUR HATS\n");
        _set_scheduler_on();
        vmm_switch(first_proc->page_dir);
        DEBUG("[KERNEL]: Jumping with EIP: %x, USERESP: %x\n", first_proc->context.eip, first_proc->context.useresp);
        enter_usermode(first_proc->context.eip, first_proc->context.useresp, first_proc->kernel_stack);
    }
}