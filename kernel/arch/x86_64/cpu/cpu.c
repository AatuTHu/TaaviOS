#include "cpu.h"
#include "apic.h"
#include "io.h"
#include "klog.h"
#include <stdbool.h>
#include <stdint.h>

static bool use_false_id = true;
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

    use_false_id = true;
}

struct core_t *cpu_current(void) {
    uint64_t p = 0;
    __asm__("mov %%gs:0, %0" : "=r"(p));
    return (struct core_t *)p;
}

// use_false_id. Spinlock tech. Cause pmm/vmm/paging/kmalloc uses spinlocks before lapic is
// initialized we cannot read the real id since lapic is not mapped. Instead just return 0.
// lapic init sets use_false_id to false and then we read the real id as it would be no onse
// bussiness
uint32_t cpu_get_lapic_id(void) {
    if (use_false_id) {
        return 0;
    }
    return cpu_current()->lapic_id;
}

void cpu_tick_ticks() {
    cpu_current()->ticks++;
}

uint64_t cpu_get_ticks() {
    return cpu_current()->ticks;
}
