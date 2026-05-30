#include <stdint.h>
#include <stddef.h>
#include "stand.h"

void main(void) {
    print("Starting shell\n");
    exec("shell");
    
}