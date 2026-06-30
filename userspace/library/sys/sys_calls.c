#include "sys_calls.h"

void sys_write(int fd, const char *msg, int len) {
    int retval;
    __asm__ __volatile__("int $0x80"
        : "=a"(retval)
        : "a"(4), "b"(fd), "c"(msg), "d"(len)
        : "memory");
}

void sys_exit(void) {
    __asm__ __volatile__("int $0x80" : : "a"(1));
}

int sys_getpid(void) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(20));
    return result;
}

int sys_open(const char *path, uint32_t flags) {
    int fd = -1;
    __asm__ __volatile__("int $0x80"
        : "=a"(fd)
        : "a"(5), "b"(path), "c"(flags));
    return fd;
}

int sys_chdir(const char *path, uint32_t len) {
    int result = -1;
    __asm__ __volatile__("int $0x80"
        : "=a"(result)
        : "a"(12), "b"(path), "c"(len));
    return result;
}

int sys_conwi(int width, int height) {
    int result = -1;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(250), "b"(width), "c"(height));
    return result;
}

int sys_mkdir(const char *path, uint32_t len) {
    int result = -1;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(39), "b"(path), "c"(len));
    return result;
}

void sys_close(uint32_t fd) {
    __asm__ __volatile__("int $0x80" : : "a"(6), "b"(fd));
}

int sys_read(int fd, char *buf, int len) {
    int result;
    __asm__ __volatile__("int $0x80"
        : "=a"(result)
        : "a"(3), "b"(fd), "c"(buf), "d"(len)
        : "memory");
    return result;
}

int sys_exec(const char *filename) {
    int result;
    __asm__ __volatile__("int $0x80"
        : "=a"(result)
        : "a"(11), "c"(filename)
        : "memory");
    return result;
}

int sys_getdirents(const char *buf, uint32_t len) {
    __asm__ __volatile__("int $0x80" : : "a"(141), "b"(buf), "c"(len));
    return 0;
}

void sys_idle(void) {
    __asm__ __volatile__("int $0x80" : : "a"(112));
}

void sys_yield(void) {
    __asm__ __volatile__("int $0x80" : : "a"(158));
}