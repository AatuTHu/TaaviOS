#ifndef KERNEL_H
#define KERNEL_H

#include "boot_info.h"
#include <stdint.h>
#define LIMINE_API_REVISION 2
#include "limine.h"

extern struct boot_info b_info;
extern void bsp_stack_switch_and_call(uint64_t new_top, void (*fn)(void));
extern void ap_stack_switch_and_call(uint64_t kstack_top, void (*func)(void), uint64_t pml4_phys);

void ignite_core(uint64_t index, uint64_t core_struct, void *goto_addr);
uint64_t kernel_stack_top(void);
void kmain();

#endif
