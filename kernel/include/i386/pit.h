#ifndef PIT_H
#define PIT_H
#include <stdint.h>

#define PIT_COMMAND_PORT 0x43
#define PIT_CHANNEL0_PORT 0x40
#define PIT_MODE_SQUARE_WAVE 0x36
#define clock_frequency 1193182

void pit_init(uint32_t frequency);
static void pit_irq_handler(void);
uint32_t pit_get_ticks(void);
uint32_t pit_get_uptime_s(void);
void pit_sleep_ms(uint32_t ms);

#endif