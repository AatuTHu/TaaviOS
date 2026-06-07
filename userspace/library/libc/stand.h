#ifndef STAND_H
#define STAND_H

#include <stddef.h>
#include <stdint.h>

#define O_RDONLY 0x01
#define O_WRONLY 0x02
#define O_RDWR   0x04
#define O_CREAT  0x08

void print(const char *msg);
void error(const char *msg);
int scan(char *buf);

int open(const char *path, uint32_t flags);
int read(int fd, char *buf, int buffer_size);
void write(int fd, const char *msg);

int exec(const char *filename);
void idle(void);
int get_pid();
void terminate_program();

int memcmp(const void *s1, const void *s2, size_t n);
#endif