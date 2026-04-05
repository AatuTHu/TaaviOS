#include "pit.h"
#include "io.h"
#include "isr.h"
#include "klog.h"

/* * The Intel 8253/8254 Programmable Interval Timer (PIT)
 * The PIT has an internal crystal oscillator running at ~1.193182 MHz.
 * It is a legacy chip, but still the standard way to get a periodic 
 * heartbeat for the system scheduler in x86 hobby kernels.
 */
#define clock_frequency 1193182

/* * WHY Volatile: The compiler might try to optimize code by assuming 
 * 'tick_count' only changes when the current function modifies it. 
 * Since 'tick_count' is changed by a hardware interrupt (asynchronously),
 * 'volatile' tells the compiler to always read the value from RAM.
 */
volatile static uint32_t tick_count = 0;
static uint32_t ticks_per_second = 0;

/**
 * Configures the PIT to fire at a specific frequency.
 * * WHY Divisor: The PIT doesn't understand "Hertz". It only understands 
 * a 16-bit counter. If we want 100Hz, we tell the PIT to count down 
 * from (1193182 / 100) to zero, then fire an interrupt and reset.
 */
void pit_init(uint32_t frequency) {
    DEBUG("Initializing PIT with frequency %d\n",frequency);
    ticks_per_second = frequency;

    // Calculate the 16-bit divisor.
    uint32_t divisor = clock_frequency / frequency;

    /* * Command Register (0x43): 
     * 0x36 = 00110110 in binary:
     * Bits 6-7 (00): Select Channel 0 (connected to IRQ0).
     * Bits 4-5 (11): Access mode (expecting low byte then high byte).
     * Bits 1-3 (011): Mode 3 (Square Wave Generator).
     * Bit 0 (0): 16-bit binary counter.
     */
    outb(0x43, 0x36);

    /* Send the divisor bytes to Channel 0 data port (0x40) */
    uint8_t lo = (uint8_t)(divisor & 0xFF);
    uint8_t hi = (uint8_t)((divisor >> 8) & 0xFF);
    outb(0x40, lo);
    outb(0x40, hi);

    /* Bind the PIT to the IDT through IRQ 0 */
    irq_register_handler(0, pit_irq_handler);
    DEBUG("PIT INITIALIZED SUCCESFULLY\n");
}

/**
 * The hardware interrupt handler for IRQ0.
 * Every time the PIT counter hits zero, the CPU pauses the current task 
 * and jumps here. This is the entry point for the Scheduler's logic.
 */
void pit_irq_handler(void) {
    tick_count++;
}

/**
 * Returns total ticks since boot. 
 * Useful for high-resolution timing or timestamping kernel events.
 */
uint32_t pit_get_ticks(void) {
    return tick_count;
}

/**
 * Returns system uptime. 
 * Note: If ticks_per_second is 100, tick_count/100 gives seconds.
 */
uint32_t pit_get_uptime_s(void) {
   return tick_count / ticks_per_second;
}

/**
 * Implements a busy-wait delay.
 * * WHY Busy-Wait: In a single-tasking environment, this is fine. 
 * However, once the scheduler is running, a better 'sleep' would 
 * block the current process and let another one run instead of 
 * spinning the CPU in this while-loop.
 */
void pit_sleep_ms(uint32_t ms) {
    uint32_t tick_count_start = tick_count;
    
    // Convert target milliseconds to the equivalent number of ticks
    uint32_t ticks_to_wait = ms * (ticks_per_second / 1000); 

    // Loop until the required time has elapsed in the background
    while ((tick_count - tick_count_start) < ticks_to_wait) {
        // We could add 'hlt' or 'pause' here to save power/cycles
        __asm__ __volatile__("pause");
    }
}