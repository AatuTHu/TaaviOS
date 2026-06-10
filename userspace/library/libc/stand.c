#include "stand.h"
#include "string.h"
#include "sys_calls.h"

void print(const char *msg) {
    sys_write(1, msg, strlen(msg));
}

void error(const char *msg) {
    sys_write(2, msg, strlen(msg));
}

int scan(char *buf) {
    return sys_read(0, buf, 1);
}

int open(const char *path, uint32_t flags) {
    return sys_open(path, flags);
}

int change_directory(const char *path, char *directory_name) {
    int len     = strnlen(path, 128);
    int respone = sys_chdir(path, len);

    if (respone != -1) {
        memcpy(directory_name, path, 8);
    }
    return respone;
}

void close(uint32_t fd) {
    sys_close(fd);
}

void write(int fd, const char *msg) {
    sys_write(fd, msg, strlen(msg));
}

int mkdir(const char *path) {
    int len      = strnlen(path, 128);
    int response = sys_mkdir(path, len);
    return response;
}

int read(int fd, char *buf, int buffer_size) {
    return sys_read(fd, buf, buffer_size);
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
