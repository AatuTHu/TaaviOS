#include "boot_info.h"
#include "config.h"
#include <stdint.h>
#define LIMINE_API_REVISION 2
#include "limine.h"
#include "serial.h"

__attribute__((used, section(".limine_requests"))) static volatile LIMINE_BASE_REVISION(3);

__attribute__((used, section(".limine_requests_start"))) static volatile LIMINE_REQUESTS_START_MARKER;

__attribute__((used, section(".limine_requests"))) static volatile struct limine_hhdm_request hhdm_req = {
    .id       = LIMINE_HHDM_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_memmap_request memmap_req = {
    .id       = LIMINE_MEMMAP_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_framebuffer_request fb_req = {
    .id       = LIMINE_FRAMEBUFFER_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_executable_address_request exec_addr_req = {
    .id       = LIMINE_EXECUTABLE_ADDRESS_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests"))) static volatile struct limine_rsdp_request rsdp_req = {
    .id       = LIMINE_RSDP_REQUEST,
    .revision = 0,
};

__attribute__((used, section(".limine_requests_end"))) static volatile LIMINE_REQUESTS_END_MARKER;

static void hcf(void) {
    __asm__ __volatile__("cli");
    serial_write("HALTING THE PROCESSOR\n");
    for (;;) {
        __asm__ __volatile__("hlt");
    }
}

static void boot_panic(const char *msg) {
    serial_write(msg);
    hcf();
}

static int boot_info_collect(struct boot_info *out) {

    if (hhdm_req.response == NULL || memmap_req.response == NULL || exec_addr_req.response == NULL) {
        return STATUS_ERROR;
    }

    out->hhdm_offset                   = hhdm_req.response->offset;
    out->kernel_phys_base              = exec_addr_req.response->physical_base;
    out->kernel_virt_base              = exec_addr_req.response->virtual_base;

    out->region_count                  = 0;
    struct limine_memmap_response *res = memmap_req.response;
    for (uint64_t i = 0; i < BOOT_MAX_REGIONS && i < res->entry_count; i++) {
        struct limine_memmap_entry *entry = res->entries[i];

        out->regions[i].base              = entry->base;
        out->regions[i].length            = entry->length;

        // reserved until proven free.
        enum boot_region_type mem_type;
        switch (entry->type) {
        case LIMINE_MEMMAP_USABLE:
            mem_type = BOOT_REGION_USABLE;
            break;
        case LIMINE_MEMMAP_RESERVED:
            mem_type = BOOT_REGION_RESERVED;
            break;
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            mem_type = BOOT_REGION_RECLAIMABLE;
            break;
        case LIMINE_MEMMAP_EXECUTABLE_AND_MODULES:
            mem_type = BOOT_REGION_KERNEL;
            break;
        case LIMINE_MEMMAP_FRAMEBUFFER:
            mem_type = BOOT_REGION_FRAMEBUFFER;
            break;
        default:
            mem_type = BOOT_REGION_RESERVED;
            break;
        }

        out->regions[i].type = mem_type;
        out->region_count++;
    }

    if (fb_req.response != NULL && fb_req.response->framebuffer_count > 0) {
        struct limine_framebuffer_response *fb_info = fb_req.response;
        out->fb.addr                                = (uint64_t)fb_info->framebuffers[0]->address;
        out->fb.height                              = fb_info->framebuffers[0]->height;
        out->fb.width                               = fb_info->framebuffers[0]->width;
        out->fb.bpp                                 = fb_info->framebuffers[0]->bpp;
        out->fb.pitch                               = fb_info->framebuffers[0]->pitch;
        out->fb.memory_model                        = fb_info->framebuffers[0]->memory_model;
        out->fb.red_mask_size                       = fb_info->framebuffers[0]->red_mask_size;
        out->fb.red_mask_shift                      = fb_info->framebuffers[0]->red_mask_shift;
        out->fb.green_mask_size                     = fb_info->framebuffers[0]->green_mask_size;
        out->fb.green_mask_shift                    = fb_info->framebuffers[0]->green_mask_shift;
        out->fb.blue_mask_size                      = fb_info->framebuffers[0]->blue_mask_size;
        out->fb.blue_mask_shift                     = fb_info->framebuffers[0]->blue_mask_shift;
    }

    if (rsdp_req.response != NULL) {
        out->rsdp = rsdp_req.response->address;
    }
    return STATUS_OK;
}

void _start(void) {

    serial_init();
    if (!LIMINE_BASE_REVISION_SUPPORTED) {
        hcf();
    }

    struct boot_info b_info = {0};
    if (boot_info_collect(&b_info) == STATUS_ERROR) {
        boot_panic("Reading boot information failed!\n");
    }

    serial_write("Sucessfully collected boot information!\n");

    hcf();
}
