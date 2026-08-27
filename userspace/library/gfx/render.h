#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

#define MAX_REGIONS 255
#define RESERVED_REGION_ID 0
#define PRIMARY_VIEWPORT_ID 0

typedef struct gfx_region_t {
    int id;
    uint32_t width;
    uint32_t height;
    uint32_t cursor_x;
    uint32_t cursor_y;
    uint32_t offset_x;
    uint32_t offset_y;
    uint32_t padding_x;
    uint32_t padding_y;
    uint32_t border_width;
    uint32_t border_color;
    uint32_t fg_color;
    uint32_t bg_color;
} gfx_region_t;

extern gfx_region_t *gfx_regions[MAX_REGIONS];

static inline void gfx_clamp_horizontal(gfx_region_t *child, gfx_region_t *parent) {
    uint32_t min_x = parent->border_width + parent->padding_x;
    if (child->cursor_x < min_x) {
        child->cursor_x += min_x;
    }
}

static inline void gfx_clamp_vertical(gfx_region_t *child, gfx_region_t *parent) {
    uint32_t min_y = parent->border_width + parent->padding_y;
    if (child->cursor_y < min_y) {
        child->cursor_y += min_y;
    }
}

int gfx_init(void);
int gfx_create_viewport(int x, int y, int w, int h, uint32_t fg_color, uint32_t bg_color);
int gfx_register_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t fg, uint32_t bg);
int gfx_resize_viewport(uint32_t region_id, int w, int h);
int gfx_move_viewport(uint32_t region_id, int x, int y);

int gfx_draw_text(uint32_t region_id, const char *text, uint32_t len);
void gfx_draw_text_at(uint32_t region_id, uint32_t x, uint32_t y, const char *msg);
void gfx_draw_text_to_region(uint32_t region_id, const char *msg);
int gfx_clear_region(uint32_t region_id);
int gfx_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color);
int gfx_draw_sprite(uint32_t region_id, int x, int y, int width, int height, uint32_t scale, uint32_t *sprite);
int gfx_reset_cursor(uint32_t region_id);

void gfx_set_width(uint32_t region_id, uint32_t w);
void gfx_set_height(uint32_t region_id, uint32_t h);
void gfx_set_padding_x(uint32_t region_id, uint32_t px);
void gfx_set_padding_y(uint32_t region_id, uint32_t py);
void gfx_set_bg_color(uint32_t region_id, uint32_t color);
void gfx_set_fg_color(uint32_t region_id, uint32_t color);
void gfx_set_border_width(uint32_t region_id, uint32_t width);
void gfx_set_border_color(uint32_t region_id, uint32_t color);

#endif
