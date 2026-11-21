#include "op_sy.h"
#include "shared.h"
#include "sys_calls.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void main(void) {

    sys_ioctl(TEST, GUI, false);
    sys_ioctl(TEST, FS, false);

    if (exec("/sysbin/shell") != 0)
        return;

    sys_yield();

    // if (exec("/sysbin/teditor") != 0)
    //     return;

    sys_yield();

    if (exec("/sysbin/fs_int") != 0)
        return;
    sys_yield();
}
