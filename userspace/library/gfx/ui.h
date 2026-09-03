#ifndef UI_H
#define UI_H

#include <stdint.h>
int resize_viewport(uint32_t width, uint32_t height);
int move_viewport(uint32_t x, uint32_t y);
int create_container(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t text_color, uint32_t background_color);
int refresh_region(uint32_t region_id);
int reset_region(uint32_t region_id);
int draw_sprite(uint32_t region_id, int x, int y, int width, int height, uint32_t scale, uint32_t *sprite);
int create_label(uint32_t width, uint32_t height, uint32_t label_x, uint32_t label_y, uint32_t text_x, uint32_t text_y, uint32_t text_color,
                 uint32_t background_color, const char *text);
int create_button(uint32_t width, uint32_t height, uint32_t x, uint32_t y, const char *title);

void show(uint32_t region_id);

int delete_region(uint32_t region_id);

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
