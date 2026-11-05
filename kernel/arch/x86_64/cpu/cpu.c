#include "cpu.h"
#include "apic.h"
#include "io.h"
#include "klog.h"
#include <stdint.h>

struct core_t cores[ACPI_MAX_CPUS];
uint8_t bsp_lapic_id;
uint8_t cpu_count;

void fill_cpu_struct(struct limine_mp_response *info) {

    bsp_lapic_id = info->bsp_lapic_id;
    cpu_count    = info->cpu_count;
    DEBUG_KERNEL("[CPU][CONSTR]: Bsp core id: %d\n", bsp_lapic_id);
    cores[0].processor_id = 0;
    cores[0].lapic_id     = bsp_lapic_id;
    cores[0].online       = 1;
    cores[0].self         = &cores[0];
    wrmsr(IA32_GS_BASE, (uint64_t)&cores[0]);

    for (uint64_t i = 0; i < info->cpu_count; i++) {
        struct limine_mp_info *cpu = info->cpus[i];
        if (cpu->lapic_id == bsp_lapic_id) {
            continue;
        }
        cores[i].lapic_id     = cpu->lapic_id;
        cores[i].processor_id = cpu->processor_id;
        cores[i].self         = &cores[i];
    }
}

struct core_t *cpu_current(void) {
    uint64_t p = 0;
    __asm__("mov %%gs:0, %0" : "=r"(p));
    return (struct core_t *)p;
}
