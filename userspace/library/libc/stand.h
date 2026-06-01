#ifndef STAND_H
#define STAND_H

#include <stdint.h>
#include <stddef.h>

void print(const char *msg);
void error(const char *msg);
int scan(char *buf);

int open(const char* path);
int read(int fd, char *buf, uint32_t count); 
void write(int fd, const char *msg);

int exec(const char *filename);
void idle(void);
int get_pid();
void terminate_program();

int memcmp(const void *s1, const void *s2, size_t n);
#endif