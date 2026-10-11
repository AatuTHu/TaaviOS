#include "boot_info.h"
#include "gdt.h"
#include "idt.h"
#include "klog.h"
#include "serial.h"
#include <stdint.h>

struct boot_info b_info;

void print_collected_info() {

    DEBUG_KERNEL("[KERNEL]: HHDM offset: 0x%x \n", b_info.hhdm_offset);
    DEBUG_KERNEL("[KERNEL]: Kernel physical base: 0x%x\n", b_info.kernel_phys_base);
    DEBUG_KERNEL("[KERNEL]: Kernel virtual base: 0x%x\n", b_info.kernel_virt_base);
    DEBUG_KERNEL("[KERNEL]: Memmap region count: %d\n", b_info.region_count);

    DEBUG_KERNEL("[KERNEL]: memmaping: \n");
    uint64_t usable_memory      = 0;
    uint64_t reclaimable_memory = 0;
    uint64_t reserved_memory    = 0;
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
                DEBUG_KERNEL("Boot reclaimable region\n");
                reclaimable_memory += b_info.regions[i].length;
                break;
            }
            DEBUG_KERNEL("\tRegion end 0x%x\n", b_info.regions[i].base + (uint64_t)b_info.regions[i].length);
        }
    }

    DEBUG_KERNEL("Total usable memory: %d bytes\n", (unsigned long long)usable_memory);
    DEBUG_KERNEL("Total reclaimable memory: %d bytes\n", (unsigned long long)reclaimable_memory);
    DEBUG_KERNEL("Total reserved memory: %d bytes\n", (unsigned long long)reserved_memory);

    DEBUG_KERNEL("Total usable memory: %d MiB\n", (unsigned long long)(usable_memory / 1048576));
    DEBUG_KERNEL("Total reclaimable memory: %d MiB\n", (unsigned long long)(reclaimable_memory / 1048576));

    if (b_info.fb.addr != 0) {
        DEBUG_KERNEL("[KERNEL]: Framebuffer information\n");
        DEBUG_KERNEL("Framebuffer addr: 0x%x\n", b_info.fb.addr);
        DEBUG_KERNEL("Framebuffer width: %d\n", b_info.fb.width);
        DEBUG_KERNEL("Framebuffer height: %d\n", b_info.fb.height);
        DEBUG_KERNEL("Framebuffer pitch: %d\n", b_info.fb.pitch);
        DEBUG_KERNEL("Framebuffer bpp: %d\n", b_info.fb.bpp);
    } else {
        DEBUG_KERNEL("No framebuffer detected\n");
    }

    DEBUG_KERNEL("[KERNEL]: rsdp address: 0x%x\n", b_info.rsdp);
}

void kmain() {

    serial_init();

    gdt_init();
    idt_init();

    int test_var = 0;

    DEBUG_KERNEL("[KERNEL][MAIN]: Address of the local variable 0x%x\n", &test_var);

    print_collected_info();

    while (1) __asm__ __volatile__("hlt");
}
