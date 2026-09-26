#include "idle_clerk.h"

/*
 * weird
 */

void idle(void) {
    while (1) __asm__ __volatile__("sti;hlt");
}
