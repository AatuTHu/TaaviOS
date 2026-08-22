#include "op_sy.h"
#include "sys_calls.h"
#include <stddef.h>
#include <stdint.h>

void main(void) {
    release_window();
    if (exec("/sysbin/shell") != 0)
        return;

    sys_yield();

    if (exec("/sysbin/fs_int") != 0)
        return;
}
