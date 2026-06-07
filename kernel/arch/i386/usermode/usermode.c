#include "usermode.h"
#include "config.h"
#include "tss.h"

void enter_usermode(uint32_t entry, uint32_t user_stack,
                    uint32_t kernel_stack) {
    tss_set_kernel_stack(kernel_stack);
    jump_to_usermode(entry, user_stack);
}