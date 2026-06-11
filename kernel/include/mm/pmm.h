#ifndef PMM_H
#define PMM_H

#include "multiboot.h"
#include <stddef.h>
#include <stdint.h>

struct mmap_entry {
    uint32_t size;
    uint32_t base_low;
    uint32_t base_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
};

void pmm_init(const struct multiboot_info *mboot);
uint32_t pmm_alloc(void);
uint32_t pmm_get_used_pages(void);
uint32_t pmm_get_free_pages(void);
void pmm_free(uint32_t addr);
void __pmm_set_bit(uint32_t page);
// int pmm_test_bit(uint32_t page);

#endif // PMM_H