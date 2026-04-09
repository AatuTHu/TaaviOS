#ifndef MM_H
#define MM_H
#include <stdint.h>
#include "config.h"

static inline uint32_t phys_to_virt(uint32_t addr) {
    return addr + KERNEL_VIRTUAL_BASE;
}

#endif