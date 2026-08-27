#include "stand.h"
#include "log.h"
#include "malloc.h"
#include "render.h"
#include "string.h"
#include "sys_calls.h"
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

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
                    LOG("Was a space: %d\n", amount_of_spaces);
                }

                if (dirents[j] != ' ') {
                    has_extension = true;
                    LOG("Has extensions\n");
                    break;
                }
            }

            if (has_extension == false) {
                dirents[i - 1] = ' ';
                continue;
            }

            LOG("amount_of_spaces: %d before extension\n", amount_of_spaces);

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

    LOG("Dirents after format: %s", dirents);

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

int list_dirents(char *buf, int buffer_size) {
    char *dirents = (char *)malloc(buffer_size);

    if (dirents == NULL) {
        LOG("Could not allocate buffer for directory entries\n");
        return STATUS_ERROR;
    }

    if (sys_getdirents(buf, buffer_size) == STATUS_ERROR) {
        LOG("Failed to read directory entries\n");
        return STATUS_ERROR;
    }

    return format_dirents(buf, buffer_size);
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

int resize_viewport(uint32_t width, uint32_t height) {
    return gfx_resize_viewport(PRIMARY_VIEWPORT_ID, width, height);
}

int move_viewport(uint32_t x, uint32_t y) {
    return gfx_move_viewport(PRIMARY_VIEWPORT_ID, x, y);
}

int create_region(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t text_color, uint32_t background_color) {
    return gfx_register_region(x, y, width, height, text_color, background_color);
}

int refresh_region(uint32_t region_id) {
    return gfx_clear_region(region_id);
}

int reset_region(uint32_t region_id) {
    return gfx_reset_cursor(region_id);
}

void set_viewport_text_color(uint32_t color) {
    gfx_set_fg_color(PRIMARY_VIEWPORT_ID, color);
}

void set_region_text_color(uint32_t region_id, uint32_t color) {
    gfx_set_fg_color(region_id, color);
}

void set_viewport_background_color(uint32_t color) {
    gfx_set_bg_color(PRIMARY_VIEWPORT_ID, color);
}

void set_region_background_color(uint32_t region_id, uint32_t color) {
    gfx_set_bg_color(region_id, color);
}

void set_viewport_padding_x(uint32_t padding) {
    gfx_set_padding_x(PRIMARY_VIEWPORT_ID, padding);
}
void set_viewport_padding_y(uint32_t padding) {
    gfx_set_padding_y(PRIMARY_VIEWPORT_ID, padding);
}
void set_region_padding_x(uint32_t region_id, uint32_t padding) {
    gfx_set_padding_x(region_id, padding);
}
void set_region_padding_y(uint32_t region_id, uint32_t padding) {
    gfx_set_padding_y(region_id, padding);
}

int draw_sprite(uint32_t region_id, int x, int y, int width, int height, uint32_t scale, uint32_t *sprite) {
    return gfx_draw_sprite(region_id, x, y, width, height, scale, sprite);
}
