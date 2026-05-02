#include <stdint.h>
#include <stddef.h>
#include "stand.h"

void main(void) {
    exec("shell");

    while(1) {
        yield_time();
    }
}