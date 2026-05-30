#ifndef STAND_H
#define STAND_H

#include <stdint.h>
#include <stddef.h>

void print(const char *msg);
void error(const char *msg);
int scan(char *buf);

//file io
int open(const char* path);
void write(int fd, const char *msg, const char *path);
int read(int fd, char *buf, const char *path);

//sys
int exec(const char *filename);
void idle(void);
int get_pid();
void terminate_program();


int memcmp(const void *s1, const void *s2, size_t n);
#endif