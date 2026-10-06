#ifndef ISR_H
#define ISR_H

#include "idt.h"
#include <stdint.h>

typedef void (*irq_callback_t)(void);

extern irq_callback_t irq_callbacks[16];
extern void syscall_handler(void);
extern void isr129(void); // scheduler yield vector

uint32_t isr_handler(struct registers *r);
uint32_t irq_handler(struct registers *r);
void irq_register_handler(int index, irq_callback_t cb);

#endif
