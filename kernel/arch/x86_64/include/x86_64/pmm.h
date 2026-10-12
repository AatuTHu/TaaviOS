#ifndef PMM_H
#define PMM_H

#include "boot_info.h"
#include <stdint.h>

int pmm_init(const struct boot_info *bi);
uint64_t pmm_alloc(void);
int pmm_free(const uint64_t phys);
uint64_t pmm_get_free_pages(void);
uint64_t pmm_get_used_pages(void);
uint64_t pmm_get_total_pages(void);
#endif
