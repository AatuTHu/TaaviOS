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

int sys_conwi(const gui_params_pack *params) {
    int result = -1;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_WI), "b"(params));
    return result;
}

int sys_fs(const fs_params_pack *params) {
    int result = -1;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_FS), "b"(params));
    return result;
}

int sys_ioctl(int operation, uintptr_t p1, uintptr_t p2) {
    int result = -1;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_IOCTL), "b"(operation), "c"(p1), "d"(p2));
    return result;
}

uintptr_t sys_sbrk(uintptr_t size) {
    int result = 0;
    __asm__ __volatile__("int $0x80" : "=a"(result) : "a"(SYS_SBRK), "b"(size));
    return result;
}

int sys_read(char *buf, int len) {
    int result;
    __asm__ __volatile__("int $0x80"
                         : "=a"(result)
                         : "a"(SYS_READ), "b"(buf), "c"(len)
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
