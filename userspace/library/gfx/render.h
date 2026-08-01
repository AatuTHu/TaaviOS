#ifndef RENDER_H
#define RENDER_H
#include <stdint.h>

int init_render();
int render(const char *text, uint32_t len);
void render_at(uint32_t x, uint32_t y, const char *msg);
void register_dimensions(uint32_t w, uint32_t h);
void set_dimensions(uint32_t w, uint32_t h);
void set_vertical_padding(uint32_t vp);
void set_horizontal_padding(uint32_t hp);
int paint_rectangle(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t color);
int create_task_window(int w, int h, int x, int y);
int resize_task_window(int w, int h);
int move_task_window(int x, int y);
int draw_buffer(int width, int height, int x, int y, uint32_t scale, uint32_t *sprite);
void set_background_color(uint32_t color);
void set_text_color(uint32_t color);

#endif
