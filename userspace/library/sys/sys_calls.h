#ifndef SYS_CALLS_H
#define SYS_CALLS_H

#include "shared.h"
#include "stdint.h"

void sys_write(int fd, const char *msg, int len);

void sys_exit(void);

int sys_getpid(void);

void sys_idle(void);

void sys_yield(void);

int sys_read(char *buf, int len);

int sys_exec(const char *filename);

int sys_fs(const fs_params_pack *params);

int sys_conwi(const gui_params_pack *params);

int sys_ioctl(int operation, uintptr_t p1, uintptr_t p2);

int sys_kill(uint32_t target_pid);

uintptr_t sys_sbrk(uintptr_t size);

#endif
