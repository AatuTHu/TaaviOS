#include "isr.h"
#include "config.h"
#include "hail_mary.h"
#include "io.h"
#include "klog.h"
#include "sched.h"
#include "task.h"

irq_callback_t irq_callbacks[16] = {0};

void irq_register_handler(int index, irq_callback_t cb) {
    DEBUG("[ISR] Registering callback %x\n", cb);
    irq_callbacks[index] = cb;
}

void isr_handler(struct registers *r) {

    if (r->int_no == 129) {
        scheduler_tick(r);
        return;
    }

    uint32_t cr2;
    __asm__ __volatile__("mov %%cr2, %0" : "=r"(cr2));

    int is_user     = (r->cs & 0x3) == 3;
    task_t *current = scheduler_get_current_task();

    if (current->task_mode == KERNEL_TASK) {
        DEBUG("[ISR]: %s made a fatal mistake. Resetting\n", current->name);
        current->state = TASK_SLEEPING;
        activate_hail_mary(current->pid);
        return;
    }

    klog("\n");
    ERROR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    ERROR("                     KERNEL PANIC                           \n");
    ERROR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    ERROR("Scheduler was running task %s\n", current->name);
    ERROR("REASON: ");
    if (r->int_no == 14) {
        ERROR("PAGE FAULT (Present: %s, Access: %s, Mode: %s)\n",
            (r->err_code & 0x1) ? "Yes" : "No",
            (r->err_code & 0x2) ? "Write" : "Read",
            (r->err_code & 0x4) ? "User" : "Kernel");
        ERROR("FAULTING ADDRESS: 0x%x\n", cr2);
    } else if (r->int_no == 13) {
        ERROR("GENERAL PROTECTION FAULT\n");
    } else {
        ERROR("EXCEPTION %d\n", r->int_no);
    }

    ERROR("LOCATION: %s mode at EIP 0x%x\n", is_user ? "USER" : "KERNEL",
        r->eip);

    ERROR("--- REGISTER DUMP ---\n");
    ERROR("EAX: 0x%x  EBX: 0x%x  ECX: 0x%x  EDX: 0x%x\n", r->eax, r->ebx,
        r->ecx, r->edx);
    ERROR("ESI: 0x%x  EDI: 0x%x  EBP: 0x%x  ESP: 0x%x\n", r->esi, r->edi,
        r->ebp, r->esp);
    ERROR("CS:  0x%x  EFLAGS: 0x%x\n", r->cs, r->eflags);

    if (is_user) {
        ERROR("USER STACK: 0x%x\n", r->useresp);
    }

    ERROR("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");
    ERROR("System halted.\n");

    while (1) { __asm__ __volatile__("hlt"); }
}

void irq_handler(struct registers *r) {

    int irq_index = r->int_no - 32;

    if (irq_index < 0 || irq_index > 15) {
        outb(0x20, 0x20);
        return;
    }

    if (irq_index == 0) {
        scheduler_tick(r);
    }

    if (irq_callbacks[irq_index] != 0) {
        irq_callbacks[irq_index]();
    }

    if (r->int_no >= 40)
        outb(0xA0, 0x20);
    outb(0x20, 0x20);
}