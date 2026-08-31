#include "idt.h"
#include "klog.h"

static inline void print_registers_to_console(struct registers *r) {
    uint32_t is_user = (r->cs & 0x3) == 3;

    DEBUG("\n--- CPU Execution State ---\n");
    DEBUG("Execution Mode  : %s Mode\n", is_user ? "User space" : "Kernel space");
    DEBUG("Instruction Ptr : 0x%x\n", r->eip);
    DEBUG("User Stack Ptr  : 0x%x\n", r->useresp);
    DEBUG("Code Segment    : 0x%x\n", r->cs);
    DEBUG("Flags Register  : 0x%x\n\n", r->eflags);

    DEBUG("--- General Purpose Registers ---\n");
    DEBUG("Accumulator (EAX)   : 0x%x\n", r->eax);
    DEBUG("Base        (EBX)   : 0x%x\n", r->ebx);
    DEBUG("Counter     (ECX)   : 0x%x\n", r->ecx);
    DEBUG("Data        (EDX)   : 0x%x\n\n", r->edx);

    DEBUG("--- Pointers & Index Registers ---\n");
    DEBUG("Source Index      (ESI) : 0x%x\n", r->esi);
    DEBUG("Destination Index (EDI) : 0x%x\n", r->edi);
    DEBUG("Base Pointer      (EBP) : 0x%x\n", r->ebp);
    DEBUG("Stack Pointer     (ESP) : 0x%x\n", r->esp);
    DEBUG("---------------------------\n\n");
}
