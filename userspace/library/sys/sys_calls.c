#include "sys_calls.h"
#include "shared.h"
#include <stdint.h>

void sys_write(int fd, const char *msg, int len) {
    int retval;
    __asm__ __volatile__("int $0x80"
                         : "=a"(retval)
                         : "a"(SYS_WRITE), "b"(fd), "c"(msg), "d"(len)
                         : "memory");
}

void sys_exit(void) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_EXIT));
}

int sys_getpid(void) {
    int result;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_GETPID));
    return result;
}

int sys_open(const char *path, uint32_t len, uint32_t flags) {
    int fd = -1;
    __asm__ __volatile__("int $0x80"
                         : "=a"(fd)
                         : "a"(SYS_OPEN), "b"(path), "c"(len), "d"(flags));
    return fd;
}

int sys_chdir(const char *path, uint32_t len) {
    int result = -1;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_CHDIR), "b"(path), "c"(len));
    return result;
}

int sys_conwi(gui_params_pack *params) {
    int result = -1;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_WI), "b"(params));
    return result;
}

int sys_ioctl(int operation, int p1, int p2) {
    int result = -1;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_IOCTL), "b"(operation), "c"(p1), "d"(p2));
    return result;
}

int sys_sbrk(uint32_t size) {
    int result = -1;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_SBRK), "b"(size));
    return result;
}

int sys_mkdir(const char *path, uint32_t len) {
    int result = -1;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_MKDIR), "b"(path), "c"(len));
    return result;
}

void sys_close(uint32_t fd) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_CLOSE), "b"(fd));
}

int sys_read(int fd, char *buf, int len) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(fd), "c"(buf), "d"(len)
                         : "memory");
    return result;
}

int sys_exec(const char *filename) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_EXEC), "c"(filename)
                         : "memory");
    return result;
}

int sys_getdirents(const char *buf, uint32_t len) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_GETDENTS), "b"(buf), "c"(len));
    return result;
}

void sys_idle(void) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_IDLE));
}

void sys_yield(void) {
    __asm__ __volatile__("int $0x80" : : "a"(SYS_YIELD));
}

int sys_kill(uint32_t target_pid) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_KILL), "b"(target_pid));
    return result;
}
