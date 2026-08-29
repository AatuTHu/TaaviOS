#ifndef STAND_H
#define STAND_H

#include <stddef.h>
#include <stdint.h>

#define STATUS_ERROR -1
#define STATUS_OK 0

void print(const char *msg);
void print_at(uint32_t region_id, uint32_t x, uint32_t y, const char *msg);
void print_to_region(uint32_t region_id, const char *msg);
void error(const char *msg);
int scan(char *buf);

int open(const char *path, uint32_t flags);
int delete_file(const char *path);
int read(int fd, char *buf, int buffer_size);
void write(int fd, const char *msg);
void close(uint32_t fd);
int mkdir(const char *path);
int change_directory(const char *path, char *directory_name);
int list_dirents(char *buf, int buffer_size);
int parse_flags_from_commands(char *command);
void idle(void);
int get_pid(void);

int resize_viewport(uint32_t width, uint32_t height);
int move_viewport(uint32_t x, uint32_t y);
int create_region(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t text_color, uint32_t background_color);
int refresh_region(uint32_t region_id);
int reset_region(uint32_t region_id);
int draw_sprite(uint32_t region_id, int x, int y, int width, int height, uint32_t scale, uint32_t *sprite);

void set_viewport_text_color(uint32_t color);
void set_viewport_background_color(uint32_t color);
void set_region_text_color(uint32_t region_id, uint32_t color);
void set_region_background_color(uint32_t region_id, uint32_t color);
void mark_cursor_position(uint32_t background_color);

void set_viewport_padding_x(uint32_t padding);
void set_viewport_padding_y(uint32_t padding);
void set_region_padding_x(uint32_t region_id, uint32_t padding);
void set_region_padding_y(uint32_t region_id, uint32_t padding);

#endif
