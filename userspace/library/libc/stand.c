#include "stand.h"
#include "sys_calls.h"
#include "string.h"

void write(const char *msg) {
    int len = strlen(msg);
    sys_write(msg, len);
}

int read(char *buf) {
    return sys_read(buf, 1);
}

void terminate_program() {
    sys_exit();
}

int exec(const char *filename) {
   return sys_exec(filename);
}

void yield_time(void) {
    sys_yield();
}
