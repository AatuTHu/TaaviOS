#include "pit.h"
#include "io.h"
#include "isr.h"
#include "config.h"
#include "klog.h"

static volatile uint32_t tick_count = 0;
static uint32_t ticks_per_second = 0;

void pit_init(uint32_t frequency) {
    DEBUG("[PIT] Initializing PIT with frequency %d\n",frequency);
    ticks_per_second = frequency;

    uint32_t divisor = clock_frequency / frequency;

    outb(PIT_COMMAND_PORT, PIT_MODE_SQUARE_WAVE);
    uint8_t lo = (uint8_t)(divisor & 0xFF);
    uint8_t hi = (uint8_t)((divisor >> 8) & 0xFF);
    outb(PIT_CHANNEL0_PORT, lo);
    outb(PIT_CHANNEL0_PORT, hi);

    irq_register_handler(0, pit_irq_handler);
    DEBUG("[PIT] PIT INITIALIZED SUCCESFULLY\n");
}

static void pit_irq_handler(void) {
    tick_count++;
}

uint32_t pit_get_ticks(void) {
    return tick_count;
}

uint32_t pit_get_uptime_s(void) {
   return tick_count / ticks_per_second;
}

void pit_sleep_ms(uint32_t ms) {
    uint32_t tick_count_start = tick_count;
    
    // Convert target milliseconds to the equivalent number of ticks
    uint32_t ticks_to_wait = ms * (ticks_per_second / 1000); 

    // Loop until the required time has elapsed in the background
    while ((tick_count - tick_count_start) < ticks_to_wait) {
        __asm__ __volatile__("pause");
    }
}