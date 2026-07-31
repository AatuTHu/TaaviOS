#include "stand.h"
#include "render.h"
#include "shared.h"
#include "string.h"
#include "sys_calls.h"
#include <stdint.h>

#define forward 0
#define backward -1
#define MAX_PATH_LEN 128
static char save_path[MAX_PATH_LEN];

static int parse_segment_from_path(const char *path, char *dir_name, int max_dir_len) {
    int len = strlen(path);

    if (len == 0) {
        int i                = 0;
        const char *root_str = "";
        while (root_str[i] != '\0' && i < (max_dir_len - 1)) {
            dir_name[i] = root_str[i];
            i++;
        }
        dir_name[i] = '\0';
        return 0;
    }

    int end = len - 1;
    if (path[end] == '/')
        end--;

    int start = end;
    while (start >= 0 && path[start] != '/') {
        start--;
    }
    start++;

    int count = 0;
    for (int i = start; i <= end && count < (max_dir_len - 1); i++) {
        dir_name[count++] = path[i];
    }
    dir_name[count] = '\0';

    return 0;
}

int change_directory(const char *path, char *directory_name) {
    int len = strlen(path);
    if (sys_chdir(path, len) == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    if (strcmp(path, "../") == 0 || strcmp(path, "..") == 0) {

        int len = strlen(save_path);
        if (len > 0) {
            int end = len - 1;
            if (save_path[end] == '/')
                end--;

            while (end >= 0 && save_path[end] != '/') {
                end--;
            }

            if (end >= 0) {
                save_path[end] = '\0';
            } else {
                save_path[0] = '\0';
            }
        }
    } else {
        strcat(save_path, path, MAX_PATH_LEN);
    }

    return parse_segment_from_path(save_path, directory_name, MAX_PATH_LEN);
}

int list_dirents(char *buf, int buffer_size) {
    return sys_getdirents(buf, buffer_size);
}

void print(const char *msg) {
    render(msg, strlen(msg));
}

void error(const char *msg) {
    sys_write(2, msg, strlen(msg));
}

int scan(char *buf) {
    return sys_read(0, buf, 1);
}

int open(const char *path, uint32_t flags) {
    return sys_open(path, strlen(path), flags);
}

void close(uint32_t fd) {
    sys_close(fd);
}

void write(int fd, const char *msg) {
    sys_write(fd, msg, strlen(msg));
}

int read(int fd, char *buf, int buffer_size) {
    return sys_read(fd, buf, buffer_size);
}

int mkdir(const char *path) {
    int len      = strnlen(path, 128);
    int response = sys_mkdir(path, len);
    return response;
}

void idle(void) {
    sys_idle();
}

int get_pid() {
    return sys_getpid();
}
