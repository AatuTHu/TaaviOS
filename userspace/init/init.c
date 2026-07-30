#include "op_sy.h"
#include <stddef.h>
#include <stdint.h>

void main(void) {
    if (exec("/sysbin/shell") != 0)
        return;
    if (exec("/sysbin/fs_int") != 0)
        return;
}
