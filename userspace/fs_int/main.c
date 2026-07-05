#include "stand.h"
#include <stddef.h>
#include <stdint.h>

void main(void) {
    if (create_task_window(600, 200, 20, 20) == -1) {
        return;
    }
    if (configurate_task_window(0, 0, 0, 0) == -1) {
        return;
    }
    print("fs_interface\n");
}