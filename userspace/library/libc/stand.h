#ifndef STAND_H
#define STAND_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define STATUS_ERROR -1
#define STATUS_OK 0

typedef struct {
    char name[13];
    uint8_t type;
    uint32_t size;
    uint32_t timestamp;
} dirent_info_t;

typedef enum {
    DIRECTORY,
    FILE,
} dirent_types;

void print(const char *msg);
void print_at(uint32_t region_id, uint32_t x, uint32_t y, const char *msg);
void print_to_region(uint32_t region_id, const char *msg);
void error(const char *msg);
int scan(char *buf);

int open(const char *path, uint32_t flags);
int delete_file(const char *path);
void write(int fd, const char *msg);
int close(uint32_t fd);
int mkdir(const char *path);
int change_directory(const char *path, char *directory_name);
int list_dirents(dirent_info_t *out_dirents, uint32_t max_dirents);
void idle(void);
int get_pid(void);
int fwrite(int fd, const char *msg, bool wfr);
int fread(int fd, char *buf, int buffer_size);

#endif
