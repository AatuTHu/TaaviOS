#include "op_sy.h"
#include "sys_calls.h"
#include <stddef.h>
#include <stdint.h>

void main(void) {

    sys_ioctl(RUN_ALL_TESTS, 0, 0);

    if (exec("/sysbin/shell") != 0)
        return;

    sys_yield();

    if (exec("/sysbin/teditor") != 0)
        return;

    sys_yield();

    //  if (exec("/sysbin/fs_int") != 0)
    //      return;
    // sys_yield();
}
