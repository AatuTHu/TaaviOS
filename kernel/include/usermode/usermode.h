#ifndef USERMODE_H
#define USERMODE_H
#include <stdint.h>

void enter_usermode(uint32_t entry, uint32_t user_stack, uint32_t kernel_stack);
extern void jump_to_usermode(uint32_t entry, uint32_t user_stack);

#endif