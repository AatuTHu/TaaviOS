#ifndef CPU_H
#define CPU_H

#include "config.h"
#include "kernel.h"
#include "tss.h"
#include <stdint.h>

struct core_t {
    struct core_t *self;
    uint32_t processor_id;
    uint32_t lapic_id;
    uint64_t kstack_top;
    volatile uint32_t online;
    struct tss_t tss;
    uint8_t df_stack[4096] __attribute__((aligned(16)));
};

extern uint8_t bsp_lapic_id;
extern uint8_t cpu_count;
extern struct core_t cores[ACPI_MAX_CPUS];

void fill_cpu_struct(struct limine_mp_response *info);
struct core_t *cpu_current(void);

#endif
