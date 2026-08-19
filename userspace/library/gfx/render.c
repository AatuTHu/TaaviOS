#include "render.h"
#include "log.h"
#include "malloc.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "sys_calls.h"
#include <math.h>
#include <stdint.h>

window_t *window_components[MAX_SECTIONS];

/**
 * TaaviOS window manager
 * Design & Implementation:
 * @author: A.H, 2026
 */

static int parse_dimensions(const char *ptr, int *w, int *h) {

    while (*ptr == ' ') ptr++;

    if (*ptr < '0' || *ptr > '9') {
        return STATUS_ERROR;
    }

    *w = 0;
    while (*ptr >= '0' && *ptr <= '9') {
        *w = *w * 10 + (*ptr - '0');
        ptr++;
    }

    while (*ptr == ' ') ptr++;

    if (*ptr != '.') {
        return STATUS_ERROR;
    }
    ptr++;
    while (*ptr == ' ') ptr++;

    if (*ptr < '0' || *ptr > '9') {
        return STATUS_ERROR;
    }

    *h = 0;
    while (*ptr >= '0' && *ptr <= '9') {
        *h = *h * 10 + (*ptr - '0');
        ptr++;
    }

    return STATUS_OK;
}

static void clamp_borders_to_window(window_t *entry) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode     = PAINT_WINDOW;
    params.struct_key = entry->window_id;

    // top border
    params.width      = entry->width;
    params.height     = entry->height;
    params.x          = 0;
    params.y          = 0;
    params.bg_color   = entry->border_color;
    sys_conwi(&params);

    params.width    = entry->width - (entry->border_width * 2);
    params.height   = entry->height - (entry->border_width * 2);
    params.x        = entry->border_width;
    params.y        = entry->border_width;
    params.bg_color = entry->bg_color;
    sys_conwi(&params);
}

int create_task_window(int w, int h, int x, int y, uint32_t foreground_color, uint32_t background_color) {

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode   = CREATE;
    params.width    = w;
    params.height   = h;
    params.x        = x;
    params.y        = y;
    params.fg_color = foreground_color;
    params.bg_color = background_color;

    //  clamp_borders_to_window(entry);

    int key         = sys_conwi(&params);

    return key;
}

int resize_task_window(int w, int h, uint32_t key) {

    char dimensions_buffer[22]; // 22 because that give 10 digits to left side and 10 to right. A dot
    // to middle and a '\0' to end.
    window_t *entry = window_components[key];

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode      = RESIZE;
    params.struct_key  = entry->window_id;
    params.width       = w;
    params.height      = h;
    params.fg_color    = entry->fg_color;
    params.bg_color    = entry->bg_color;
    params.buf         = dimensions_buffer;
    params.buffer_size = 22;
    int result         = sys_conwi(&params);

    if (result == -1) {
        return result;
    }

    const char *ptr = params.buf;

    parse_dimensions(ptr, &w, &h);

    entry->width  = w;
    entry->height = h;

    // clamp_borders_to_window(entry);

    return result;
}

int paint_section(uint32_t key) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    window_t *entry   = window_components[key];

    params.opcode     = PAINT_WINDOW;
    params.struct_key = entry->window_id;
    params.width      = entry->width;
    params.height     = entry->height;
    params.x          = entry->cursor_x_pos;
    params.y          = entry->cursor_y_pos;
    params.bg_color   = entry->bg_color;
    params.fg_color   = entry->fg_color;

    return sys_conwi(&params);
}

int paint_rectangle(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t color, uint32_t key) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    window_t *entry        = window_components[key];

    uint32_t double_border = entry->border_width * 2;

    uint32_t max_w         = (entry->width > double_border) ? (entry->width - double_border) : 0;
    uint32_t max_h         = (entry->height > double_border) ? (entry->height - double_border) : 0;

    if (x < entry->border_width) {
        x = entry->border_width;
    }
    if (y < entry->border_width) {
        y = entry->border_width;
    }

    if (x >= entry->width - entry->border_width) {
        width = 0;
    } else if (x + width > entry->width - entry->border_width) {
        width = (entry->width - entry->border_width) - x;
    }

    if (y >= entry->height - entry->border_width) {
        height = 0;
    } else if (y + height > entry->height - entry->border_width) {
        height = (entry->height - entry->border_width) - y;
    }

    if (width > max_w)
        width = max_w;
    if (height > max_h)
        height = max_h;

    params.opcode     = PAINT_WINDOW;
    params.struct_key = entry->window_id;
    params.width      = width;
    params.height     = height;
    params.x          = x;
    params.y          = y;
    params.bg_color   = color;
    params.fg_color   = entry->fg_color;

    return sys_conwi(&params);
}

int move_task_window(int x, int y, uint32_t key) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    window_t *entry   = window_components[key];
    params.opcode     = MOVE;
    params.struct_key = entry->window_id;
    params.x          = x;
    params.y          = y;
    params.fg_color   = entry->fg_color;
    params.bg_color   = entry->bg_color;

    //    clamp_borders_to_window();

    return sys_conwi(&params);
}

int draw_buffer(int width, int height, int x, int y, uint32_t scale, uint32_t *sprite, uint32_t key) {
    window_t *entry = window_components[key];
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode     = DRAW;
    params.struct_key = entry->window_id;
    params.width      = width;
    params.height     = height;
    params.x          = x;
    params.y          = y;
    params.scale      = scale;
    params.pixels     = sprite;
    params.fg_color   = entry->fg_color;
    params.bg_color   = entry->bg_color;
    return sys_conwi(&params);
}

int register_section(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t foreground_color, uint32_t background_color) {

    int slot = -1;

    for (int i = RESERVED_SLOT; i < MAX_SECTIONS; i++) {
        if (window_components[i] == NULL) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        LOG("Could not find free slot from the window\n");
        return STATUS_ERROR;
    }

    window_t *parent = window_components[MAIN_WINDOW_KEY];

    if (parent == NULL) {
        LOG("Cant register a section if there is not parent window\n");
        return STATUS_ERROR;
    }

    window_t *new_entry = (window_t *)malloc(sizeof(window_t));

    if (new_entry == NULL) {
        LOG("Could not allocate memory for new section entry\n");
        return STATUS_ERROR;
    }

    new_entry->window_id              = parent->window_id;
    new_entry->width                  = width;
    new_entry->height                 = height;
    new_entry->section_x_pos          = x;
    new_entry->section_y_pos          = y;
    new_entry->cursor_x_pos           = x;
    new_entry->cursor_y_pos           = y;
    new_entry->border_width           = 0;
    new_entry->def_horizontal_padding = 0;
    new_entry->def_vertical_padding   = 0;
    new_entry->bg_color               = background_color;
    new_entry->fg_color               = foreground_color;

    window_components[slot]           = new_entry;

    LOG("Added section to slot: %d\n", slot);
    LOG("width: %d\n", width);
    LOG("height: %d\n", height);
    LOG("x: %d\n", x);
    LOG("y: %d\n", y);
    // paint_rectangle(width, height, x, y, background_color, slot);

    return slot;
}

int init_render() {
    // LOG("Renderer initialized\n");
    int window_id = create_task_window(300, 150, 10, 10, COLOR_WHITE, COLOR_BLACK);

    if (window_id == STATUS_ERROR) {
        //    LOG("Failed to create window\n");
        return STATUS_ERROR;
    }

    window_t *entry = (window_t *)malloc(sizeof(window_t));

    if (entry == NULL) {
        LOG("Cant allocate entry for the main window\n");
        return STATUS_ERROR;
    }

    // LOG("Initializing renderer\n");
    entry->window_id                 = window_id;
    entry->border_color              = COLOR_DARKER_GRAY;
    entry->border_width              = 4;
    entry->def_vertical_padding      = 1 + entry->border_width;
    entry->def_horizontal_padding    = 1 + entry->border_width;
    entry->width                     = 200;
    entry->height                    = 100;
    entry->cursor_x_pos              = entry->def_horizontal_padding;
    entry->cursor_y_pos              = entry->def_vertical_padding;
    entry->fg_color                  = COLOR_WHITE;
    entry->bg_color                  = COLOR_BLACK;

    window_components[RESERVED_SLOT] = entry;

    // clamp_borders_to_window();

    return STATUS_OK;
}
