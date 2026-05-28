#include "stand.h"
#include "sys_calls.h"
#include "string.h"

void print(const char *msg) {
    sys_write(msg, strlen(msg), 1);
}

void error(const char *msg) {
    sys_write(msg, strlen(msg), 2);
}

int scan(char *buf) {
    return sys_read(buf, 1, 0);
}

int open(const char* path) {
    return sys_open(path);
}

void write(int fd, const char *msg) {
    sys_write(msg, strlen(msg), fd);
}

int read(int fd, char *buf) {
    return sys_read(buf, strlen(buf) ,fd);
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
