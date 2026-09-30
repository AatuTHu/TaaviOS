#include "op_sy.h"
#include "shared.h"
#include "sys_calls.h"
#include <stddef.h>
#include <stdint.h>

void main(void) {

    sys_ioctl(TEST, TEST_CREATE_WINDOW, 0);
    sys_ioctl(TEST, TEST_RESIZE_WINDOW, 0);
    sys_ioctl(TEST, TEST_MOVE_WINDOW, 0);

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
