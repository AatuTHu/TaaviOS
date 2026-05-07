#ifndef STAND_H
#define STAND_H

#include <stdint.h>
#include <stddef.h>

void write(const char *msg);
int read(char *buf);
int exec(const char *filename);
int get_pid();
int memcmp(const void *s1, const void *s2, size_t n);
void terminate_program();
void idle(void);
#endif