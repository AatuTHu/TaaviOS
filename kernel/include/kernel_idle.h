#ifndef KERNEL_IDLE_H
#define KERNEL_IDLE_H

static inline void kernel_idle(void) {
    while(1) __asm__ __volatile__("hlt");
}

#endif