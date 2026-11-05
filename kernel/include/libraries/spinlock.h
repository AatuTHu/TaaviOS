#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>

typedef struct {
    volatile uint32_t locked;
    volatile uint32_t owner;
    const char *name;
} spinlock_t;

#define SPINLOCK_INIT(n) {0, 0, (n)}
#define SPIN_LIMIT 200000000UL
#define RFLAGS_IF (1ULL << 9)

void spin_light_lock(spinlock_t *l);
void spin_light_unlock(spinlock_t *l);

uintptr_t spin_lock_disable_all_intr(spinlock_t *l);
void spin_unlock_restore_all_intr(spinlock_t *l, uintptr_t flags);

#endif
