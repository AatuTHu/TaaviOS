#include "ui.h"
#include "font.h"
#include "log.h"
#include "malloc.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include <stdint.h>

container_info_t *container_list[MAX_REGIONS];

static int force_to_fit_and_register_region(uint32_t width, uint32_t height, uint32_t x, uint32_t y, const char *str, uint32_t txt_col, uint32_t bg_col) {

    if (str == NULL) {
        return STATUS_ERROR;
    }

    uint32_t str_width = strlen(str) * FONT_WIDTH;

    if (width < str_width) {
        width = str_width;
    }

    if (height < FONT_HEIGHT) {
        height = FONT_HEIGHT;
    }

    return gfx_register_region(x, y, width, height, txt_col, bg_col, str);
}

int create_button(uint32_t width, uint32_t height, uint32_t x, uint32_t y, const char *title, void (*cb)(void)) {

    int button_id = force_to_fit_and_register_region(width, height, x, y, title, COLOR_WHITE, COLOR_DEEP_BLUE);

    if (button_id == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    for (int i = 0; i < MAX_REGIONS; i++) {
        if (container_list[i] == NULL) {
            container_info_t *entry = (container_info_t *)malloc(sizeof(container_info_t));
            if (entry == NULL) {
                gfx_delete_region(button_id);
                return STATUS_ERROR;
            }

            entry->region_id  = button_id;
            entry->cb         = cb;
            container_list[i] = entry;
            break;
        }
    }

    gfx_region_t *button = gfx_regions[button_id];

    button->border_width = 2;
    button->border_color = COLOR_DARKER_GRAY;

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

    int label_id = force_to_fit_and_register_region(width, height, label_x, label_y, text, text_color, background_color);

    if (label_id == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    gfx_clear_region(label_id);
    gfx_region_t *label = gfx_regions[label_id];

    label->cursor_x     = text_x;
    label->cursor_y     = text_y;

    return label_id;
}

void show(uint32_t region_id) {
    if (region_id >= MAX_REGIONS) {
        return;
    }

    if (gfx_regions[region_id]->border_width > 0) {
        gfx_draw_borders(region_id);
    } else {
        gfx_clear_region(region_id);
    }

    if (gfx_regions[region_id]->str != NULL) {
        gfx_region_t *region = gfx_regions[region_id];
        gfx_draw_text_at(region_id, region->cursor_x, region->cursor_y, region->str);
    }
}

void button_press(uint32_t region_id) {
    for (int i = 0; i < MAX_REGIONS; i++) {
        if (container_list[i] != NULL && container_list[i]->region_id == region_id) {
            container_list[i]->cb();
            return;
        }
    }
}

int resize_viewport(uint32_t width, uint32_t height) {
    return gfx_resize_viewport(PRIMARY_VIEWPORT_ID, width, height);
}

int move_viewport(uint32_t x, uint32_t y) {
    return gfx_move_viewport(PRIMARY_VIEWPORT_ID, x, y);
}

int reset_region(uint32_t region_id) {
    if (gfx_clear_region(region_id) == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    return gfx_reset_cursor(region_id);
}

/**
 * set_viewport_text_color - change the primary viewport's foreground/text color.
 * @color: new foreground color.
 *
 * Description:
 * Directly sets the fg_color field on the primary viewport region.
 *
 * Return: void.
 */
void set_viewport_text_color(uint32_t color) {
    gfx_regions[PRIMARY_VIEWPORT_ID]->fg_color = color;
}

/**
 * set_region_text_color - change a region's foreground/text color.
 * @region_id: region component to modify.
 * @color: new foreground color.
 *
 * Description:
 * Directly sets the fg_color field on the given region component.
 *
 * Return: void.
 */
void set_region_text_color(uint32_t region_id, uint32_t color) {
    gfx_regions[region_id]->fg_color = color;
}

/**
 * set_viewport_background_color - change the primary viewport's background color.
 * @color: new background color.
 *
 * Description:
 * Directly sets the bg_color field on the primary viewport region.
 *
 * Return: void.
 */
void set_viewport_background_color(uint32_t color) {
    gfx_regions[PRIMARY_VIEWPORT_ID]->bg_color = color;
}

/**
 * set_region_background_color - change a region's background color.
 * @region_id: region component to modify.
 * @color: new background color.
 *
 * Description:
 * Directly sets the bg_color field on the given region component.
 *
 * Return: void.
 */
void set_region_background_color(uint32_t region_id, uint32_t color) {
    gfx_regions[region_id]->bg_color = color;
}

/**
 * set_viewport_padding_x - set left/right padding for the primary viewport.
 * @padding: new horizontal padding, excluding border.
 *
 * Description:
 * Adds the viewport's existing border width to the requested padding.
 *
 * Return: void.
 */
void set_viewport_padding_x(uint32_t padding) {
    gfx_regions[PRIMARY_VIEWPORT_ID]->padding_x = padding + gfx_regions[PRIMARY_VIEWPORT_ID]->border_width;
}

/**
 * set_viewport_padding_y - set top/bottom padding for the primary viewport.
 * @padding: new vertical padding, excluding border.
 *
 * Description:
 * Adds the viewport's existing border width to the requested padding.
 *
 * Return: void.
 */
void set_viewport_padding_y(uint32_t padding) {
    gfx_regions[PRIMARY_VIEWPORT_ID]->padding_y = padding + gfx_regions[PRIMARY_VIEWPORT_ID]->border_width;
}

/**
 * set_region_padding_x - set left/right padding for a region.
 * @region_id: region component to modify.
 * @padding: new horizontal padding, excluding border.
 *
 * Description:
 * Adds the component's existing border width to the requested padding,
 * so callers only need to think in terms of padding beyond the border.
 *
 * Return: void.
 */
void set_region_padding_x(uint32_t region_id, uint32_t padding) {
    if (region_id == PRIMARY_VIEWPORT_ID) {
        gfx_regions[region_id]->padding_x = padding + gfx_regions[region_id]->border_width;
        return;
    }
    gfx_region_t *parent              = gfx_regions[PRIMARY_VIEWPORT_ID];
    gfx_regions[region_id]->padding_x = padding + gfx_regions[region_id]->border_width + parent->border_width + parent->padding_x;
}

/**
 * set_region_padding_y - set top/bottom padding for a region.
 * @region_id: region component to modify.
 * @padding: new vertical padding, excluding border.
 *
 * Description:
 * Adds the component's existing border width to the requested padding,
 * so callers only need to think in terms of padding beyond the border.
 *
 * Return: void.
 */
void set_region_padding_y(uint32_t region_id, uint32_t padding) {
    if (region_id == PRIMARY_VIEWPORT_ID) {
        gfx_regions[region_id]->padding_y = padding + gfx_regions[region_id]->border_width;
        return;
    }
    gfx_region_t *parent              = gfx_regions[PRIMARY_VIEWPORT_ID];
    gfx_regions[region_id]->padding_y = padding + gfx_regions[region_id]->border_width + parent->border_width + parent->padding_y;
}

int draw_sprite(uint32_t region_id, int x, int y, int width, int height, uint32_t scale, uint32_t *sprite) {
    return gfx_draw_sprite(region_id, x, y, width, height, scale, sprite);
}

void mark_cursor_position(uint32_t x, uint32_t y, uint32_t background_color) {
    gfx_fill_rect(x, y, 1, FONT_HEIGHT - 3, background_color);
}

int delete_container(uint32_t region_id) {
    for (int i = 0; i < MAX_REGIONS; i++) {
        if (container_list[i] != NULL && container_list[i]->region_id == region_id) {
            free(container_list[i]);
            container_list[i] = NULL;
            return gfx_delete_region(region_id);
        }
    }

    return STATUS_ERROR;
}

void delete_all_containers() {
    for (int i = 0; i < MAX_REGIONS; i++) {
        if (container_list[i] != NULL) {
            free(container_list[i]);
            container_list[i] = NULL;
            break;
        }
    }
}

/**
 * set_region_border_color - change a region's border color.
 * @region_id: region component to modify.
 * @color: new border color.
 *
 * Description:
 * Directly sets the border_color field on the given region component.
 * Repaints the border to make the change visible immediately via show().
 *
 * Return: void.
 */
void set_region_border_color(uint32_t region_id, uint32_t color) {

    if (gfx_regions[region_id] == NULL) {
        LOG("Invalid region ID\n");
        return;
    }

    gfx_regions[region_id]->border_color = color;
    show(region_id);
}

int get_region_cursor_x(uint32_t region_id) {
    if (gfx_regions[region_id] != NULL) {
        return gfx_regions[region_id]->cursor_x;
    }

    return 0;
}
