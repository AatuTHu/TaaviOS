
#include "smp.h"
#include "apic.h"
#include "config.h"
#include "cpu.h"
#include "gdt.h"
#include "idt.h"
#include "io.h"
#include "kernel.h"
#include "klog.h"
#include "paging.h"
#include "sched.h"
#include "vmm.h"
#include <stdint.h>

static volatile uint32_t cpu_active_count = 0;

// ap_main()
//
// The kmain of the apmain(?).
//
// force gdt and idt pointers down the toilet, enable lapic, increment active cpu count.
// initialize scheduler fields for the core, start ticking and yield to lumbridge.
//
static void ap_main() {

    struct core_t *c = cpu_current();

    gdt_flush_core(c);
    idt_load();
    lapic_enable_local();

    __atomic_add_fetch(&cpu_active_count, 1, __ATOMIC_SEQ_CST);
    c->online = 1;

    DEBUG_KERNEL("[SMP][AP_MAIN] cpu %d online, lapic id %d\n", c->processor_id, c->lapic_id);
    scheduler_init();
    scheduler_set_current_task(idle_task_pid);
    _set_scheduler_on();

    lapic_timer_start_periodic(10);

    scheduler_yield(NULL);

    while (1) {
        __asm__ __volatile__("sti; hlt");
    }
}

// ap_warmup
// Now the core has been ignited but it needs some warmth. To gather some heat we save its entry to
// kspace and uspace gs msr, calculate kernel_page_dirs physical addr and call a magic func that
// changes the limine kstack to previosly allocated kstack, changes cr3 reg to kernel_page_dir and
// faaaaaaar jumps to ap main.
//
static void ap_warmup(struct limine_mp_info *info) {
    struct core_t *c_entry = (struct core_t *)info->extra_argument;
    // save to both uspace and kspace gs the core struct so that it can be alwasys found when
    // spwapped the geeäs during iterrupts
    wrmsr(IA32_GS_BASE, (uint64_t)c_entry);
    wrmsr(IA32_KERNEL_GS_BASE, (uint64_t)c_entry);
    uint64_t pml4_phys = paging_get_directory_phys_addr(&kernel_page_dir);
    ap_stack_switch_and_call(c_entry->kstack_top, ap_main, pml4_phys);
    __builtin_unreachable();
}

// ap_setup
//
// Main launcher for ap cores.
// loop over the amount of cpus (cores, change name?)
// Allocate a kstack for it, initialize gdt and ignite it. (ignite gives goto addr for the core so
// it can advance from limine loop)
//
//
static void ap_setup() {
    uint64_t cpu_index = 1;

    for (uint64_t i = 0; i < cpu_count; i++) {

        if (cpu_index >= cpu_count) {
            break;
        }

        struct core_t *c_entry = &cores[cpu_index];

        if (c_entry->online == 1) {
            DEBUG_KERNEL("[SMP][WAKE_AP]: cpu already running, skipping it\n");
            continue;
        }

        if (vmm_alloc_kstack(&c_entry->kstack_top) == STATUS_ERROR) {
            ERROR("[SMP][INIT]: Failed to map kernel stack for the cpu core %d\n", c_entry->lapic_id);
            continue;
        }
        gdt_init_core(c_entry);
        ignite_core(cpu_index, (uint64_t)c_entry, ap_warmup);
        cpu_index++;
    }

    volatile uint64_t loops = 0;
    while (cpu_active_count != cpu_index) {
        if (++loops > MAX_WAIT_LOOPS) {
            break;
        }
        __asm__ __volatile__("pause");
    }
}

int smp_init() {
    DEBUG_KERNEL("[SMP][INIT]: Initializing all cores and starting symmetric mutiprocessing\n");

    cpu_active_count = 1;
    ap_setup();

    if (cpu_active_count < cpu_count) {
        ap_setup();
    }

    DEBUG_KERNEL("[SMP][INIT]: Current cores running %d out of %d\n", cpu_active_count, cpu_count);
    return STATUS_OK;
}
