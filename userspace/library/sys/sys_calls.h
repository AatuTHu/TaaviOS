#ifndef SYS_CALLS_H
#define SYS_CALLS_H

void sys_write(const char *msg, int len);

void sys_exit(void);

int sys_getpid(void);

void sys_idle(void);

void sys_yield(void);

int sys_read(char *buf, int len);

int sys_exec(const char *filename);

#endif