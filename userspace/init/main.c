#include <stdint.h>
#include <stddef.h>
#include "stand.h"

void main(void) {
    exec("shell");
    //exec("shell2");
    while(1) {
        idle();
    }
}