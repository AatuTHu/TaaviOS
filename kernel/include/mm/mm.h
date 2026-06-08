#ifndef MM_H
#define MM_H
#include "config.h"
#include <stdint.h>

static inline uint32_t phys_to_virt(uint32_t addr) {
    return addr + KERNEL_VIRTUAL_BASE;
}

static inline uint32_t virt_to_phys(uint32_t addr) {
    return addr - KERNEL_VIRTUAL_BASE;
}

#endif