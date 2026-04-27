#include "stand.h"
#include "sys_calls.h"
#include "string.h"

void write(const char *msg) {
    int len = strlen(msg);
    sys_write(msg, len);
}

int read(char *buf) {
    int len = strlen(buf);
    return sys_read(buf, len);
}

void terminate_program() {
    sys_exit();
}

int exec(const char *filename) {
   return sys_exec(filename);
}
