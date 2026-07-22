#ifndef STAND_H
#define STAND_H

#include <stddef.h>
#include <stdint.h>

#define STATUS_ERROR -1
#define STATUS_OK 0

void print(const char *msg);
void error(const char *msg);
int scan(char *buf);

int open(const char *path, uint32_t flags);
int read(int fd, char *buf, int buffer_size);
void write(int fd, const char *msg);
void close(uint32_t fd);
int mkdir(const char *path);
int change_directory(const char *path, char *directory_name);
int list_dirents(char *buf, int buffer_size);

void idle(void);
int get_pid();

#endif