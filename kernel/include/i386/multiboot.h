#ifndef MULTIBOOT_H
#define MULTIBOOT_H

#include <stdint.h>

struct multiboot_info {
    uint32_t flags;       // 0x00 0
    uint32_t mem_lower;   // 0x04 4
    uint32_t mem_upper;   // 0x08 8
    uint32_t boot_device; // 0x0C 12
    uint32_t cmdline;     // 0x10 16
    uint32_t mods_count;  // 0x14 20
    uint32_t mods_addr;   // 0x18 24
    uint32_t syms[4];     // 0x1C - 0x28 28, 32, 36, 40
    uint32_t mmap_length; // 0x2C 44
    uint32_t mmap_addr;   // 0x30 48
};

struct multiboot_mod {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
};

#endif