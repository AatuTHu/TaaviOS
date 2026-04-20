static inline void kernel_idle(void) {
    while(1) __asm__ __volatile__("hlt");
}