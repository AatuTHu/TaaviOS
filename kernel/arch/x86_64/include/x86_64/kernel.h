#ifndef KERNEL_H
#define KERNEL_H

#define LIMINE_API_REVISION 2
#include "boot_info.h"
#include "limine.h"
#define KERNEL_STACK_SIZE (64 * 1024)

extern struct boot_info b_info;

uint64_t kernel_stack_top(void);
void kmain();

#endif
