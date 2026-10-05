#include "op_sy.h"
#include "shared.h"
#include "sys_calls.h"
#include <stddef.h>
#include <stdint.h>

void main(void) {

    // sys_ioctl(TEST, GUI_CLERK, 0);
    sys_ioctl(TEST, FS_CLERK, 0);

    if (exec("/sysbin/shell") != 0)
        return;

    sys_yield();

    // if (exec("/sysbin/teditor") != 0)
    //     return;

    sys_yield();

    //  if (exec("/sysbin/fs_int") != 0)
    //      return;
    // sys_yield();
}
