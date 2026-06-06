#include <stdint.h>
#include <stddef.h>
#include "stand.h"

void main(void) {
    //print("Starting shell\n");
    if(exec("shell") != 0) return;
}