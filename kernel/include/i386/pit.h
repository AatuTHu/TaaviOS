#ifndef PIT_H
#define PIT_H
#include <stdint.h>


void pit_init(uint32_t frequency);
void pit_irq_handler(void);
uint32_t pit_get_ticks(void);
uint32_t pit_get_uptime_s(void);
void pit_sleep_ms(uint32_t ms);

#endif