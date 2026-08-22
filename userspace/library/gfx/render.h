#ifndef RENDER_H
#define RENDER_H
#include <stdint.h>

#define MAX_SECTIONS 255
#define RESERVED_SLOT 0
#define MAIN_WINDOW_KEY 0

typedef struct window_t {
    int window_id;
    uint32_t def_horizontal_padding;
    uint32_t def_vertical_padding;
    uint32_t width;
    uint32_t height;
    uint32_t cursor_x_pos;
    uint32_t cursor_y_pos;
    uint32_t section_x_pos;
    uint32_t section_y_pos;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t border_width;
    uint32_t border_color;
} window_t;

extern window_t *window_components[MAX_SECTIONS];

int init_render();
int render(const char *text, uint32_t len);
void render_at(uint32_t x, uint32_t y, const char *msg);
void render_at_section(uint32_t x, uint32_t y, const char *msg, uint32_t section_key);
int paint_rectangle(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t color);
int paint_section(uint32_t key);
int create_task_window(int w, int h, int x, int y, uint32_t foreground_color, uint32_t background_color);
int resize_task_window(int w, int h, uint32_t key);
int move_task_window(int x, int y, uint32_t key);
int draw_buffer(int width, int height, int x, int y, uint32_t scale, uint32_t *sprite, uint32_t key);
int register_section(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t foreground_color, uint32_t background_color);

void set_width(uint32_t w, uint32_t key);
void set_height(uint32_t h, uint32_t key);
void set_horizontal_padding(uint32_t hp, uint32_t key);
void set_vertical_padding(uint32_t vp, uint32_t key);
void set_background_color(uint32_t color, uint32_t key);
void set_text_color(uint32_t color, uint32_t key);
void set_border_width(uint32_t width, uint32_t key);
void set_border_color(uint32_t color, uint32_t key);
#endif
