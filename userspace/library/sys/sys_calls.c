#include "sys_calls.h"

void sys_write(const char *msg, int len) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(4), "b"(1), "c"(msg), "d"(len)
        : "memory"
    );
}

void sys_exit(void) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(1)
    );
}

int sys_getpid(void) {
    int result;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(20)
    );
    return result;
}

int sys_read(char *buf, int len) {
    int result;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(3), "b"(0), "c"(buf), "d"(len)
        : "memory"
    );
    return result;
}

int sys_exec(const char *filename) {
    int result;
    __asm__ __volatile__(
        "int $0x80"
        : "=a" (result)
        : "a"(11),"c"(filename)
        : "memory"
    );
    return result;
}

void sys_yield(void) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(158)
    );
}