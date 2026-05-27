#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include <stddef.h>

struct multiboot_info {
    uint32_t flags;          // 0x00 0
    uint32_t mem_lower;      // 0x04 4
    uint32_t mem_upper;      // 0x08 8
    uint32_t boot_device;    // 0x0C 12
    uint32_t cmdline;        // 0x10 16
    uint32_t mods_count;     // 0x14 20
    uint32_t mods_addr;      // 0x18 24
    uint32_t syms[4];        // 0x1C - 0x28 28, 32, 36, 40
    uint32_t mmap_length;    // 0x2C 44
    uint32_t mmap_addr;      // 0x30 48
};

struct mmap_entry {
    uint32_t size;
    uint32_t base_low;
    uint32_t base_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
};

struct multiboot_mod {
    uint32_t mod_start;
    uint32_t mod_end;
    uint32_t string;
    uint32_t reserved;
};

void pmm_init(struct multiboot_info *mboot);
uint32_t pmm_alloc(void);
uint32_t pmm_get_used_pages(void);
uint32_t pmm_get_free_pages(void);
void pmm_free(uint32_t addr);
void __pmm_set_bit(uint32_t page);
//int pmm_test_bit(uint32_t page);

#endif  // PMM_H