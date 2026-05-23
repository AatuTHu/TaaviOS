#include "idle_task.h"

void idle(void) {
    while(1) __asm__ __volatile__("sti; hlt");
}