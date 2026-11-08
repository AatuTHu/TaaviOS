#include "spinlock.h"
#include <stdint.h>
#if defined(__x86_64__)
#include "apic.h"
#include "cpu.h"
#endif
#include "kstring.h"
#include "serial.h"

static uintptr_t irq_save(void) {
    uintptr_t rflags = 0;
#if defined(__x86_64__)
    __asm__ __volatile__("pushfq ; popq %0" : "=r"(rflags)::"memory");
#else
    __asm__ __volatile__("pushf ; pop %0" : "=r"(rflags)::"memory");
#endif
    __asm__ __volatile__("cli" ::: "memory");
    return rflags;
}

static void irq_restore(uintptr_t flags) {
    if (flags & RFLAGS_IF) {
        __asm__ __volatile__("sti" ::: "memory");
    }
}

void spin_light_lock(spinlock_t *l) {
    uintptr_t spin_laps = 0;
    while (__atomic_exchange_n(&l->locked, 1, __ATOMIC_ACQUIRE) == 1) {
        while (__atomic_load_n(&l->locked, __ATOMIC_RELAXED) != 0) {
            if (++spin_laps > SPIN_LIMIT) {
                char cpuid[10];
#if defined(__x86_64__)
                itoa(cpu_get_lapic_id(), cpuid);
#else
                itoa(0, cpuid);
#endif
                serial_write("[SPIN_LOCK]: Spin limit surpassed by ");
                serial_write(cpuid);
                serial_write(l->name);
                while (1) __asm__ __volatile__("cli; hlt");
            }
            __builtin_ia32_pause();
        }
    }
#if defined(__x86_64__)
    l->owner = cpu_get_lapic_id();
#else
    l->owner = 0;
#endif
}

void spin_light_unlock(spinlock_t *l) {
    if (l->locked == 0) {
        serial_write("Tried to double unlock the spunlock");
        while (1) __asm__ __volatile__("cli; hlt");
    }
    l->owner = 0;
    __atomic_store_n(&l->locked, 0, __ATOMIC_RELEASE);
}

uintptr_t spin_lock_disable_all_intr(spinlock_t *l) {
    uintptr_t flags = irq_save();
    spin_light_lock(l);
    return flags;
}

void spin_unlock_restore_all_intr(spinlock_t *l, uintptr_t flags) {
    spin_light_unlock(l);
    irq_restore(flags);
}
