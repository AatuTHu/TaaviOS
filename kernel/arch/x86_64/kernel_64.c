#include "acpi.h"
#include "apic.h"
#include "ata.h"
#include "boot_info.h"
#include "config.h"
#include "doc_clerk.h"
#include "elf.h"
#include "fat32.h"
#include "fb.h"
#include "fs_clerk.h"
#include "gdt.h"
#include "gui_clerk.h"
#include "idle_clerk.h"
#include "idt.h"
#include "keyboard.h"
#include "klog.h"
#include "kmalloc.h"
#include "mbr.h"
#include "paging.h"
#include "pmm.h"
#include "reaper_clerk.h"
#include "sched.h"
#include "serial.h"
#include "smp.h"
#include "syscall.h"
#include "task.h"
#include "tss.h"
#include "vmm.h"
#include <stdint.h>

struct boot_info b_info;
task_t *task = NULL;

static void panic() {
    while (1) __asm__ __volatile__("hlt");
}

static void print_collected_info() {

    DEBUG_KERNEL("[KERNEL]: HHDM offset: 0x%x \n", b_info.hhdm_offset);
    DEBUG_KERNEL("[KERNEL]: Kernel physical base: 0x%x\n", b_info.kernel_phys_base);
    DEBUG_KERNEL("[KERNEL]: Kernel virtual base: 0x%x\n", b_info.kernel_virt_base);
    DEBUG_KERNEL("[KERNEL]: Memmap region count: %d\n", b_info.region_count);

    DEBUG_KERNEL("[KERNEL]: memmaping: \n");
    uint64_t usable_memory      = 0;
    uint64_t reclaimable_memory = 0;
    uint64_t reserved_memory    = 0;
    uint64_t bad_memory         = 0;
    if (b_info.region_count > 0) {
        for (uint64_t i = 0; i < b_info.region_count; i++) {
            DEBUG_KERNEL("Region index %d\n", i);
            DEBUG_KERNEL("\tRegion base: 0x%x\n", b_info.regions[i].base);
            DEBUG_KERNEL("\tRegion length: %d\n", b_info.regions[i].length);
            DEBUG_KERNEL("\tRegion type: ");

            switch (b_info.regions[i].type) {
            case BOOT_REGION_KERNEL:
                DEBUG_KERNEL("kernel region\n");
                reserved_memory += b_info.regions[i].length;
                break;
            case BOOT_REGION_USABLE:
                DEBUG_KERNEL("usable region\n");
                usable_memory += b_info.regions[i].length;
                break;
            case BOOT_REGION_RESERVED:
                DEBUG_KERNEL("reserved region\n");
                reserved_memory += b_info.regions[i].length;
                break;
            case BOOT_REGION_FRAMEBUFFER:
                DEBUG_KERNEL("framebuffer region\n");
                reserved_memory += b_info.regions[i].length;
                break;
            case BOOT_REGION_RECLAIMABLE:
                DEBUG_KERNEL("boot reclaimable region\n");
                reclaimable_memory += b_info.regions[i].length;
                break;
            case BOOT_REGION_ACPI_NVS:
                DEBUG_KERNEL("acpi NVS region\n");
                reserved_memory += b_info.regions[i].length;
                break;
            case BOOT_REGION_BAD_MEMORY:
                DEBUG_KERNEL("Bad memory region\n");
                bad_memory += b_info.regions[i].length;
                break;
            case BOOT_REGION_ACPI_RECLAIMABLE:
                DEBUG_KERNEL("acpi reclaimamble region\n");
                reclaimable_memory += b_info.regions[i].length;
            }
            DEBUG_KERNEL("\tRegion end 0x%x\n", b_info.regions[i].base + (uint64_t)b_info.regions[i].length);
        }
    }

    DEBUG_KERNEL("Total usable memory: %d bytes\n", (unsigned long long)usable_memory);
    DEBUG_KERNEL("Total reclaimable memory: %d bytes\n", (unsigned long long)reclaimable_memory);
    DEBUG_KERNEL("Total reserved memory: %d bytes\n", (unsigned long long)reserved_memory);
    DEBUG_KERNEL("Total bad memory: %d bytes\n", (unsigned long long)bad_memory);

    DEBUG_KERNEL("Total usable memory: %d MiB\n", (unsigned long long)(usable_memory / 1048576));
    DEBUG_KERNEL("Total reclaimable memory: %d MiB\n", (unsigned long long)(reclaimable_memory / 1048576));

    if (b_info.fb.addr != 0) {
        DEBUG_KERNEL("[KERNEL]: Framebuffer information\n");
        DEBUG_KERNEL("- Framebuffer addr: 0x%x\n", b_info.fb.addr);
        DEBUG_KERNEL("- Framebuffer width: %d\n", b_info.fb.width);
        DEBUG_KERNEL("- Framebuffer height: %d\n", b_info.fb.height);
        DEBUG_KERNEL("- Framebuffer pitch: %d\n", b_info.fb.pitch);
        DEBUG_KERNEL("- Framebuffer bpp: %d\n", b_info.fb.bpp);
    } else {
        DEBUG_KERNEL("No framebuffer detected\n");
    }

    DEBUG_KERNEL("[KERNEL]: rsdp address: 0x%x\n", b_info.rsdp);

    DEBUG_KERNEL("[KERNEL]: kernel module count: %d\n", b_info.module_count);
    for (uint64_t i = 0; i < b_info.module_count; i++) {
        struct boot_modules entry = b_info.modules[i];

        DEBUG_KERNEL("- Module address: 0x%x\n", entry.address);
        DEBUG_KERNEL("- Module size: %d\n", entry.size);
        DEBUG_KERNEL("- Module path: %s\n", entry.path);
        DEBUG_KERNEL("- Module cmdline: %s\n", entry.cmdline);
        DEBUG_KERNEL("- Module mediatype: %d\n", entry.media_type);
        DEBUG_KERNEL("- Module partition index: %d\n", entry.partition_index);
        DEBUG_KERNEL("- Module mbr disk index: %d\n", entry.mbr_disk_id);
    }
}

static void init_drivers() {
    if (fb_init_limine(&b_info) == STATUS_ERROR) {
        ERROR("[KERNEL]: Failed to initialize virtual memory\n");
        panic();
    }

    if (keyboard_init() == STATUS_ERROR) {
        ERROR("[KERNEL]: Failed to initialize keyborad driver\n");
        panic();
    }

    ata_init();
}

static void init_filesystems() {
    uint32_t fat32_lba     = 0;
    uint32_t fat32_sectors = 0;

    for (int i = 0; i < 4; i++) {
        ata_drive_t *drive = ata_get_drive(i);
        if (drive == NULL) {
            continue;
        }
        if (mbr_find_fat32(drive, &fat32_lba, &fat32_sectors) == STATUS_OK) {
            DEBUG_KERNEL("[MBR]: Partition found, initializing filesystem\n");
            fat32_init(drive, fat32_lba);
            break;
        }
    }
}

static void init_microlithic() {
    DEBUG_KERNEL("[KERNEL]: --INIT KERNEL TASKS--\n");
    DEBUG_KERNEL("[KERNEL]: Creating an idle kernel task\n");
    task_t *kernel_task = task_create(idle_task_pid, (uint64_t)idle, 0,
                                      "idle", &kernel_page_dir, KERNEL_TASK);

    if (kernel_task != NULL) {
        scheduler_add(kernel_task);
    }

    DEBUG_KERNEL("[KERNEL]: Creating an filesystem kernel task\n");
    kernel_task = task_create(fs_clerk_pid, (uint64_t)fs_task_loop, 0,
                              "fs_task", &kernel_page_dir, KERNEL_TASK);

    if (kernel_task != NULL) {
        fs_init(kernel_task);
        scheduler_add(kernel_task);
    }

    DEBUG_KERNEL("[KERNEL]: Creating an reaper kernel task\n");
    kernel_task = task_create(reaper_clerk_pid, (uint64_t)reaper_task_loop, 0,
                              "reaper", &kernel_page_dir, KERNEL_TASK);

    if (kernel_task != NULL) {
        reaper_init(kernel_task);
        scheduler_add(kernel_task);
    }

    DEBUG_KERNEL("[KERNEL]: Creating an graphical user interface kernel task\n");
    kernel_task = task_create(gui_clerk_pid, (uint64_t)gui_task_loop, 0,
                              "gui_task", &kernel_page_dir, KERNEL_TASK);

    if (kernel_task != NULL) {
        gui_init(kernel_task);
        scheduler_add(kernel_task);
    }

    DEBUG_KERNEL("[KERNEL]: Creating a system doctor clerk\n");
    kernel_task = task_create(doc_clerk_pid, (uint64_t)doc_clerk_loop, 0,
                              "doc_clerk", &kernel_page_dir, KERNEL_TASK);

    if (kernel_task != NULL) {
        doc_init(kernel_task);
        scheduler_add(kernel_task);
    }

    ledger_init();
}

void load_modules() {
    for (uint64_t i = 0; i < b_info.module_count; i++) {

        struct boot_modules entry = b_info.modules[i];

        page_directory_t *init_pd = vmm_create_dir();

        if (init_pd == NULL) {
            ERROR("Failed to create directory for the module. Aborting\n");
            return;
        }

        uint64_t init_phys   = entry.address;
        uint64_t heap_start  = 0;
        uint64_t entry_point = elf_load((void *)init_phys, init_pd, &heap_start);
        if (entry_point == 0) {
            ERROR("Failed to load the elf file. Aborting\n");
            continue;
        }
        DEBUG_KERNEL("[KERNEL]: ELF loaded at 0x%x.\n", entry_point);
        DEBUG_KERNEL("[KERNEL]: Creating init task\n");
        task = task_create(-1, entry_point, heap_start, "init", init_pd, USER_TASK);
        // first_task->priority = PRIORITY_HIGH;

        if (task == NULL) {
            if (vmm_free_user_space(init_pd) == STATUS_ERROR) {
                ERROR("[TASK]: Failed to free virtual page directory\n");
            }
        } else {
            scheduler_add(task);
        }
    }
}

void kmain() {

    serial_init();
    gdt_init();
    idt_init();
    print_collected_info();

    if (pmm_init(&b_info) == STATUS_ERROR) {
        ERROR("[KERNEL]: Failed to initialize physical memory manager. ABORTING\n");
        panic();
    }

    if (paging_init(&b_info) == STATUS_ERROR) {
        ERROR("[KERNEL]: Failed to initialize paging\n");
        panic();
    }

    if (kmalloc_init((void *)HEAP_START, ADDITION_HEAP_PAGE_SIZE * PAGE_SIZE) == STATUS_ERROR) {
        ERROR("[KERNEL]: Failed to initialize heap memory pool\n");
        panic();
    }

    if (run_acpi_protocol(&b_info) == STATUS_ERROR) {
        ERROR("[KERNEL]: Failed to initialize acpi\n");
        panic();
    }

    if (apic_init() == STATUS_ERROR) {
        ERROR("[KERNEL]: Failed to initialize apic\n");
        panic();
    }

    init_drivers();
    init_filesystems();
    init_microlithic();
    load_modules();

    /* if (smp_init() == STATUS_ERROR) {
         ERROR("[KERNEL]: Failed to initialize SMP\n");
         panic();
     }*/

    pmm_reclaim_bootloader(&b_info);

    scheduler_init();
    syscall_init();

    lapic_timer_start_periodic(10);
    DEBUG_KERNEL("[KERNEL]: Ready to RUN SOME TASKS\n");

    if (task != NULL) {
        DEBUG_KERNEL("[KERNEL]: ENTERING USERMODE HOLD ON TO YOUR HATS\n");
        scheduler_set_current_task(idle_task_pid);
        _set_scheduler_on();
        scheduler_yield(NULL);
    }

    panic();
}
