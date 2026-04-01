#ifndef MM_H
#define MM_H
#include <stdint.h>

static inline uint32_t phys_to_virt(uint32_t addr) {
    return addr + 0xC0000000;
}

#endif