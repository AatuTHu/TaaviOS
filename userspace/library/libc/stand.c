#include "stand.h"
#include "sys_calls.h"
#include "string.h"

void write(const char *msg) {
    sys_write(msg, strlen(msg), 1);
}

void fwrite(int fd, const char *msg) {
    sys_write(msg, strlen(msg), 4);
}

int read(char *buf) {
    return sys_read(buf, 1);
}

int fread(char *buf) {
    return sys_read(buf, 1);
}

void terminate_program() {
    sys_exit();
}

int exec(const char *filename) {
   return sys_exec(filename);
}

void idle(void) {
    sys_idle();
}

int get_pid() {
    return sys_getpid();
}
