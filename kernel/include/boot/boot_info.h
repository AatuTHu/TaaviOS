#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include <stddef.h>
#include <stdint.h>

#define BOOT_MAX_REGIONS 128

enum boot_region_type {
    BOOT_REGION_USABLE,
    BOOT_REGION_RESERVED,
    BOOT_REGION_RECLAIMABLE,
    BOOT_REGION_KERNEL,
    BOOT_REGION_FRAMEBUFFER,
};

struct boot_mem_region {
    uint64_t base;
    uint64_t length;
    enum boot_region_type type;
};

struct boot_framebuffer {
    uint64_t addr;
    uint32_t width;
    uint32_t height;
    uint32_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
};

struct boot_info {
    uint64_t hhdm_offset;
    uint64_t kernel_phys_base;
    uint64_t kernel_virt_base;
    uint64_t rsdp;
    struct boot_framebuffer fb;
    size_t region_count;
    struct boot_mem_region regions[BOOT_MAX_REGIONS];
};

#endif
