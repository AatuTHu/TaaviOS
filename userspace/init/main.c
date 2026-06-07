#include "stand.h"
#include <stddef.h>
#include <stdint.h>

void main(void) {
    // print("Starting shell\n");
    if (exec("shell") != 0)
        return;
}