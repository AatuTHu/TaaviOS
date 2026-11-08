#ifndef ISR_H
#define ISR_H

#include "idt.h"
#include <stdint.h>

typedef void (*irq_callback_t)(void);
void irq_register_handler(int index, irq_callback_t cb);
extern irq_callback_t irq_callbacks[16];
extern void syscall_handler();
extern void isr255();
extern void *irq32;
extern void isr129(); // scheduler yield vector
uint64_t irq_handler(struct registers *r);
uint64_t isr_handler(struct registers *r);

#endif
