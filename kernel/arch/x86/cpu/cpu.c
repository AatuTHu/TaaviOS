#include "cpu.h"
#include "config.h"
#include "io.h"
#include <stdbool.h>
#include <stdint.h>

struct core_t cores[MAX_CPUS];
uint8_t bsp_lapic_id;
uint8_t cpu_count;

struct core_t *this_core() {
    return &cores[0];
}

void init_cores() {

    bsp_lapic_id          = 0;
    cpu_count             = 1;
    cores[0].lapic_id     = 0;
    cores[0].online       = 1;
    cores[0].processor_id = 0;
}
