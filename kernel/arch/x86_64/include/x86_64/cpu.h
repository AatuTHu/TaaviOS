#ifndef CPU_H
#define CPU_H

#include "gdt.h"
#include "kernel.h"
#include <stdint.h>

struct core_t {
    uint32_t processor_id;
    uint32_t lapic_id;
    uint64_t kstack_top;
    volatile uint32_t online;
    struct tss_t tss;
    uint8_t df_stack[4096] __attribute__((aligned(16)));
};

extern uint8_t bsp_lapic_id;
void fill_cpu_struct(struct limine_mp_response *info);

#endif
