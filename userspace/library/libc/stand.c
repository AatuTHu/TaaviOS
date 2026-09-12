#include "stand.h"
#include "log.h"
#include "malloc.h"
#include "render.h"
#include "string.h"
#include "sys_calls.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#define MAX_SEGMENT_LEN 64
#define forward 0
#define backward -1
#define MAX_PATH_LEN 128
static char save_path[MAX_PATH_LEN];

static int format_dirents(char *dirents, int dirents_size) {
    if (dirents_size <= 0 || dirents == NULL) {
        LOG("Dirents not found\n");
        return STATUS_OK;
    }

    for (int i = 0; i < dirents_size; i++) {

        if (dirents[i] == '\0') {
            break;
        }

        if (dirents[i] == ' ') {
            dirents[i] = '.';
            i++;
            int amount_of_spaces = 0;
            bool has_extension   = false;
            int j                = i;
            for (j = i; j < dirents_size; j++) {
                if (dirents[j] == '\0' || dirents[j] == '\n') {
                    break;
                }

                if (dirents[j] == ' ') {
                    amount_of_spaces++;
                }

                if (dirents[j] != ' ') {
                    has_extension = true;
                    break;
                }
            }

            if (has_extension == false) {
                dirents[i - 1] = ' ';
                continue;
            }

            if (amount_of_spaces == 0) {
                continue;
            }

            for (j = i; j < dirents_size; j++) {
                if (dirents[j] == '\0') {
                    break;
                }
                dirents[j] = dirents[j + amount_of_spaces];
            }
        }
    }

    return strlen(dirents);
}

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

int list_dirents(dirent_info_t *out_dirents, uint32_t max_dirents) {
    char buf[512]   = {0};
    uint32_t slot   = 0;
    int buffer_size = sys_getdirents(buf, sizeof(buf) - 1);

    if (buffer_size <= 0) {
        LOG("Failed to read directory entries\n");
        return STATUS_ERROR;
    }

    buf[buffer_size] = '\0';
    int line_start   = 0;

    for (int i = 0; i <= buffer_size && slot < max_dirents; i++) {
        if (buf[i] == '\n' || buf[i] == '\0') {
            if (i > line_start) {
                buf[i] = '\0';

                memset(&out_dirents[slot], 0, sizeof(dirent_info_t));

                char *segment_start = &buf[line_start];

                for (int j = line_start; j <= i; j++) {
                    if (buf[j] == '/' || buf[j] == '\0') {
                        buf[j] = '\0';

                        while (*segment_start == ' ' || *segment_start == '\t') {
                            segment_start++;
                        }

                        if (*segment_start != '\0') {

                            if (str_starts_with(segment_start, "ID:") == 1) {
                                out_dirents[slot].type = (segment_start[3] == 'D') ? DIRECTORY : FILE;
                            }

                            if (str_starts_with(segment_start, "N:") == 1) {
                                char *name_src = segment_start + 2;

                                format_dirents(name_src, strlen(name_src));

                                int idx = 0;
                                while (idx != strlen(name_src)) {
                                    if (name_src[idx] != ' ') {
                                        out_dirents[slot].name[idx] = name_src[idx];
                                    }
                                    idx++;
                                }
                                out_dirents[slot].name[idx] = '\0';
                            }

                            if (str_starts_with(segment_start, "S:") == 1) {
                                char *size_src         = segment_start + 2;
                                out_dirents[slot].size = atoi(size_src);
                            }
                        }

                        segment_start = &buf[j + 1];
                    }
                }

                slot++;
            }
            line_start = i + 1;
        }
    }

    return slot;
}
void print(const char *msg) {
    gfx_draw_text(PRIMARY_VIEWPORT_ID, msg, strlen(msg));
}

void print_at(uint32_t region_id, uint32_t x, uint32_t y, const char *msg) {
    gfx_draw_text_at(region_id, x, y, msg);
}

void print_to_region(uint32_t region_id, const char *msg) {
    gfx_draw_text_to_region(region_id, msg);
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

int delete_file(const char *path) {
    return sys_unlink(path);
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
