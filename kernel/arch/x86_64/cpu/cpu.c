#include "cpu.h"
#include "klog.h"
#include <stdint.h>

struct core_t cores[16];
uint8_t bsp_lapic_id;

void fill_cpu_struct(struct limine_mp_response *info) {

    bsp_lapic_id = info->bsp_lapic_id;
    DEBUG_KERNEL("[CPU][CONSTR]: Bsp core id: %d\n", bsp_lapic_id);
    for (uint64_t i = 0; i < info->cpu_count; i++) {
        struct limine_mp_info *cpu = info->cpus[i];
        DEBUG_KERNEL("[CPU][CONSTR]: Lapic_id: %d \n", cpu->lapic_id);
        DEBUG_KERNEL("[CPU][CONSTR]: Processor_id: %d \n", cpu->processor_id);

        cores[i].lapic_id     = cpu->lapic_id;
        cores[i].processor_id = cpu->processor_id;
    }
}
