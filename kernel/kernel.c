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
#include "task.h"
#include "sched.h"
#include "usermode.h"
#include "elf.h"
#include "syscall.h"
#include "keyboard.h"
#include "ata.h"
#include "io.h"
#include "mbr.h"
#include "fat32.h"
#include "fs_task.h"
#include "idle_task.h"
#include "reaper_task.h"

#define MAX_MODS 10
uint32_t module_starts[MAX_MODS];
uint32_t module_ends[MAX_MODS];
uint32_t total_mods = 0;

static void init_serial_and_vga() {
    vga_init();
    serial_init();
    DEBUG("[KERNEL]: --INIT SERIAL & VGA--\n");
}

static void init_arch() {
    DEBUG("[KERNEL]: --INIT ARCH--\n");
    gdt_init();
    tss_init();
    idt_init();
    paging_init();

    DEBUG("[KERNEL]: Allocating kernel page directory\n");
    vmm_alloc(&kernel_page_dir, HEAP_START, HEAP_PAGES * PAGE_SIZE, PAGE_PRESENT | PAGE_RW);
}

static void init_mm(const uint32_t *mboot_info) {
    DEBUG("[KERNEL]: --INIT MEMORY MANAGEMENT--\n");
    const struct multiboot_info *mboot = (struct multiboot_info *)phys_to_virt((uint32_t)mboot_info);
    pmm_init(mboot);
}

static void check_for_modules (const uint32_t *mboot_info) {
    DEBUG("[KERNEL]: --CHECKING FOR MODULES--\n");
    const struct multiboot_info *mbi = (struct multiboot_info *)phys_to_virt((uint32_t)mboot_info);
    
    if (mbi->mods_count > 0) {
        const struct multiboot_mod *mods = (struct multiboot_mod *)phys_to_virt(mbi->mods_addr);
        total_mods = mbi->mods_count;

        for (uint32_t i = 0; i < total_mods && i < MAX_MODS; i++) {
            module_starts[i] = mods[i].mod_start;
            module_ends[i] = mods[i].mod_end;

            uint32_t start_page = mods[i].mod_start / PAGE_SIZE;
            uint32_t end_page = (mods[i].mod_end + PAGE_SIZE - 1) / PAGE_SIZE;
            for (uint32_t p = start_page; p < end_page; p++) {
                __pmm_set_bit(p);
            }
        }
    } else {
        DEBUG("[KERNEL]: MODULES NOT FOUND\n");
    }
}

static void init_drivers() {
    keyboard_init();
    ata_init();
    
}
        
static void init_filesystems() {
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

static void init_kernel_tasks() {
    DEBUG("[KERNEL]: --INIT KERNEL TASKS--\n");
    DEBUG("[KERNEL]: Creating an idle kernel task\n");
    task_t *kernel_task = task_create(idle_task_pid,(uint32_t)idle, "idle", &kernel_page_dir, KERNEL_TASK);

    if(kernel_task != NULL) {
        scheduler_add(kernel_task);
    }
    
    DEBUG("[KERNEL]: Creating an filesystem kernel task\n");
    kernel_task = task_create(fs_task_pid,(uint32_t)fs_task_loop, "fs_task", &kernel_page_dir, KERNEL_TASK);
    
    if(kernel_task != NULL) {
        fs_init(kernel_task);
        scheduler_add(kernel_task);
    }
    
    DEBUG("[KERNEL]: Creating an reaper kernel task\n");
    kernel_task = task_create(reaper_task_pid,(uint32_t)reaper_task_loop, "reaper_task", &kernel_page_dir, KERNEL_TASK);
    
    if(kernel_task != NULL) {
        reaper_init(kernel_task);
        scheduler_add(kernel_task);
    }
}


void kernel_main(const uint32_t *mboot_info) {

    set_print_level(2);

    init_serial_and_vga();

    init_mm(mboot_info);
    check_for_modules(mboot_info);
    
    init_arch();
    kmalloc_init((void*)HEAP_START, HEAP_PAGES * PAGE_SIZE);
    
    init_drivers();
    init_filesystems();
    
    
    scheduler_init();
    syscall_init();
    init_kernel_tasks();
    
    task_t *first_task = NULL;

    if(total_mods > 0) {
        page_directory_t *init_pd = vmm_create_directory();
        uint32_t init_phys = module_starts[0];
        uint32_t entry = elf_load((void*)phys_to_virt(init_phys), init_pd);
        if(entry != ET_NONE) {
            DEBUG("[KERNEL]: ELF loaded.\n");
            DEBUG("[KERNEL]: Creating init task\n");
            first_task = task_create(-1,entry, "init", init_pd, USER_TASK);
            //first_task->priority = PRIORITY_HIGH;
            scheduler_add(first_task);
        } else {
            DEBUG("[KERNEL]: ELF load failed!\n");
        }
        
    }

    if(total_mods > 1) {
        page_directory_t *shell_pd = vmm_create_directory();
        uint32_t shell_phys = module_starts[1];
        uint32_t entry = elf_load((void*)phys_to_virt(shell_phys), shell_pd);    
        if (entry != ET_NONE) {
            task_create(-1,entry, "shell", shell_pd, USER_TASK);
        }
    }

    pit_init(PIT_FREQUENCY);
    
    __asm__ __volatile__("sti");
    
    if(first_task != NULL) {
        DEBUG("[KERNEL]: ENTERING USERMODE HOLD ON TO YOUR HATS\n");
        scheduler_set_current_task(first_task->pid);
        _set_scheduler_on();
        vmm_switch(first_task->page_dir);
        DEBUG("[KERNEL]: Jumping with EIP: %x, USERESP: %x, NAME: %s\n", first_task->context.eip, first_task->context.useresp, first_task->name);
        enter_usermode(first_task->context.eip, first_task->context.useresp, first_task->kernel_stack);
    }
}