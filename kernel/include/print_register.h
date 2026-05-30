#include "idt.h"
#include "klog.h"

static inline void print_registers_to_console(struct registers r) {
    uint32_t is_user = (r.cs & 0x3) == 3;
    DEBUG("LOCATION: %s mode at EIP 0x%x\n", is_user ? "USER" : "KERNEL", r.eip);
    DEBUG("--- REGISTER DUMP ---\n");
    DEBUG("EAX: 0x%x  EBX: 0x%x  ECX: 0x%x  EDX: 0x%x\n", r.eax, r.ebx, r.ecx, r.edx);
    DEBUG("ESI: 0x%x  EDI: 0x%x  EBP: 0x%x  ESP: 0x%x\n", r.esi, r.edi, r.ebp, r.esp);
    DEBUG("CS:  0x%x  EFLAGS: 0x%x\n", r.cs, r.eflags);
    DEBUG("USER STACK: 0x%x\n", r.useresp);
}