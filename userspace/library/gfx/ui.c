#include "ui.h"
#include "font.h"
#include "log.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include <stdint.h>

int create_button(uint32_t width, uint32_t height, uint32_t x, uint32_t y, const char *title) {

    if (title == NULL) {
        return STATUS_ERROR;
    }

    uint32_t title_width = strlen(title) * FONT_WIDTH;

    if (width < title_width) {
        width = title_width;
    }

    if (height < FONT_HEIGHT) {
        height = FONT_HEIGHT;
    }

    int button_id = gfx_register_region(x, y, width, height, COLOR_WHITE, COLOR_DEEP_BLUE, title);

    if (button_id == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    gfx_region_t *button = gfx_regions[button_id];

    gfx_clamp_horizontal(button, gfx_regions[PRIMARY_VIEWPORT_ID]);
    gfx_clamp_vertical(button, gfx_regions[PRIMARY_VIEWPORT_ID]);

    button->cursor_x += (button->width / 2) - ((strlen(title) * FONT_WIDTH / 2));
    button->cursor_y += (button->height - FONT_HEIGHT) / 2;

    return button_id;
}

int create_container(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t text_color, uint32_t background_color) {

    int region_id = gfx_register_region(x, y, width, height, text_color, background_color, NULL);

    if (region_id == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    if (gfx_clear_region(region_id) == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    return region_id;
}

int create_label(uint32_t width, uint32_t height, uint32_t label_x, uint32_t label_y, uint32_t text_x, uint32_t text_y, uint32_t text_color, uint32_t background_color, const char *text) {
    if (text == NULL) {
        return STATUS_ERROR;
    }

    uint32_t text_width = strlen(text) * FONT_WIDTH;

    if (width < text_width) {
        width = text_width;
    }

    if (height < FONT_HEIGHT) {
        height = FONT_HEIGHT;
    }

    int label_id = gfx_register_region(label_x, label_y, width, height, text_color, background_color, text);

    if (label_id == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    gfx_clear_region(label_id);
    gfx_region_t *label = gfx_regions[label_id];

    gfx_clamp_horizontal(label, gfx_regions[PRIMARY_VIEWPORT_ID]);
    gfx_clamp_vertical(label, gfx_regions[PRIMARY_VIEWPORT_ID]);
    label->cursor_x = text_x;
    label->cursor_y = text_y;

    return label_id;
}

void show(uint32_t region_id) {
    if (region_id > MAX_REGIONS) {
        return;
    }

    gfx_clear_region(region_id);

    if (gfx_regions[region_id]->str != NULL) {
        gfx_region_t *region = gfx_regions[region_id];
        gfx_draw_text_at(region_id, region->cursor_x, region->cursor_y, region->str);
    }
}

int resize_viewport(uint32_t width, uint32_t height) {
    return gfx_resize_viewport(PRIMARY_VIEWPORT_ID, width, height);
}

int move_viewport(uint32_t x, uint32_t y) {
    return gfx_move_viewport(PRIMARY_VIEWPORT_ID, x, y);
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

void mark_cursor_position(uint32_t background_color) {
    gfx_paint_cursor_position(background_color);
}
