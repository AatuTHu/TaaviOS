#ifndef CPU_H
#define CPU_H

#include "paging.h"
#include <stdint.h>

#define MAX_CPUS 1

struct sched_info {
    int current_pid;
    int prev_pid;
    int idle_pid;
    int high_prio_pick_count;
    uint8_t scheduler_on;
    page_directory_t *current_dir;
};

struct core_t {
    struct sched_info sched;
    uint32_t processor_id;
    uint32_t lapic_id;
    volatile uint32_t online;
};

extern uint8_t bsp_lapic_id;
extern uint8_t cpu_count;
extern struct core_t cores[MAX_CPUS];

struct core_t *this_core(void);
void init_cores();

#endif
