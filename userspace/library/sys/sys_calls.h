#ifndef SYS_CALLS_H
#define SYS_CALLS_H

#include "stdint.h"

void sys_write(int fd, const char *msg, int len);

void sys_exit(void);

void sys_close(uint32_t fd);

int sys_getpid(void);

void sys_idle(void);

int sys_mkdir(const char *path, uint32_t len);

void sys_yield(void);

int sys_read(int fd, char *buf, int len);

int sys_open(const char *path, uint32_t flags);

int sys_chdir(const char *path, uint32_t len);

int sys_exec(const char *filename);

int sys_getdirents(const char *buf, uint32_t len);

#endif