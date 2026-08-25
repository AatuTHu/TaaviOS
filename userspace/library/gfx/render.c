#include "render.h"
#include "log.h"
#include "malloc.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "sys_calls.h"
#include <signal.h>
#include <stdint.h>

window_t *window_components[MAX_SECTIONS];

/**
 * TaaviOS window manager
 * Design & Implementation:
 * @author: A.H, 2026
 */

/**
 * parse_dimensions - parse a "W.H" formatted string into width/height.
 * @ptr: buffer containing the dimension string.
 * @w: output width.
 * @h: output height.
 *
 * Description:
 * Reads leading digits as width, skips a '.' separator, then reads
 * trailing digits as height, tolerating surrounding spaces.
 *
 * Return: STATUS_OK || STATUS_ERROR.
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

/**
 * clamp_borders_to_window - repaint a window's border and interior.
 * @key: window component whose border should be repainted.
 *
 * Description:
 * Paints the full window area with the border color, then paints the
 * inner area (inset by border_width on each side) with the background
 * color, producing a visible border frame.
 *
 * Return: void.
 */
static void clamp_borders_to_window(uint32_t key) {
    gui_params_pack params;

    window_t *entry = window_components[key];

    if (entry == NULL) {
        return;
    }

    memset(&params, 0, sizeof(params));
    params.opcode     = PAINT_WINDOW;
    params.struct_key = entry->window_id;

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

/**
 * create_task_window - request a new top-level window from the kernel.
 * @x: x position of the window.
 * @y: y position of the window.
 * @w: width of the window.
 * @h: height of the window.
 * @fg_color: foreground color.
 * @bg_color: background color.
 *
 * Description:
 * Sends a CREATE request; the kernel allocates the window and returns
 * its key. This key is then passed pack to kernel when making changes to window
 * so that it can correctly identify which window to modify
 *
 * Return: new window key, or STATUS_ERROR.
 */
int create_task_window(int x, int y, int w, int h, uint32_t fg_color, uint32_t bg_color) {

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode   = CREATE;
    params.width    = w;
    params.height   = h;
    params.x        = x;
    params.y        = y;
    params.fg_color = fg_color;
    params.bg_color = bg_color;

    //  clamp_borders_to_window(entry);

    return sys_conwi(&params);
}

/**
 * resize_task_window - request a resize of an existing window.
 * @key: window component to resize.
 * @w: requested width.
 * @h: requested height.
 *
 * Description:
 * Sends a RESIZE request and parses the kernel's confirmed "W . H"
 * response back into the component, then repaints its border to match.
 *
 * Kernel gives back new w and h because there is a chance it has to clamp
 * the requested paint inside the main window.
 *
 * Return: result of sys_conwi.
 */
int resize_task_window(uint32_t key, int w, int h) {

    char dimensions_buffer[22]; // 22 because that give 10 digits to left side and 10 to right. A dot
    // to middle and a '\0' to end.
    window_t *entry = window_components[key];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

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

    clamp_borders_to_window(key);

    return result;
}

/**
 * paint_section - repaint a window component using its own metadata.
 * @key: window component to repaint.
 *
 * Description:
 * Paints the component's full area at its current section position using
 * its own background and foreground colors.
 *
 * Return: result of sys_conwi.
 */
int paint_section(uint32_t key) {
    window_t *entry = window_components[key];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

    gui_params_pack params;
    memset(&params, 0, sizeof(params));

    params.opcode     = PAINT_WINDOW;
    params.struct_key = entry->window_id;
    params.width      = entry->width;
    params.height     = entry->height;
    params.x          = entry->section_x_pos;
    params.y          = entry->section_y_pos;
    params.bg_color   = entry->bg_color;
    params.fg_color   = entry->fg_color;

    return sys_conwi(&params);
}

/**
 * paint_rectangle - paint a color-filled rectangle within the main window.
 * @x: x position of the rectangle.
 * @y: y position of the rectangle.
 * @width: requested width.
 * @height: requested height.
 * @color: fill color.
 *
 * Description:
 * Clamps the requested rectangle to stay inside the main window's border,
 * shrinking width/height (or zeroing them) if the position or size would
 * otherwise overflow the drawable area.
 *
 * Return: result of sys_conwi, or STATUS_ERROR if there is no main window.
 */
int paint_rectangle(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    window_t *entry = window_components[MAIN_WINDOW_KEY];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

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

/**
 * move_task_window - request a move of an existing window.
 * @key: window component to move.
 * @x: new x position.
 * @y: new y position.
 *
 * Description:
 * Sends a MOVE request using the component's own colors.
 *
 * Return: result of sys_conwi.
 */
int move_task_window(uint32_t key, int x, int y) {
    window_t *entry = window_components[key];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode     = MOVE;
    params.struct_key = entry->window_id;
    params.x          = x;
    params.y          = y;
    params.fg_color   = entry->fg_color;
    params.bg_color   = entry->bg_color;

    return sys_conwi(&params);
}

/**
 * draw_buffer - blit a pixel buffer into a window component.
 * @key: window component to draw into.
 * @x: x position of the draw.
 * @y: y position of the draw.
 * @width: buffer width.
 * @height: buffer height.
 * @scale: scale factor applied to the sprite.
 * @sprite: raw pixel buffer.
 *
 * Description:
 * Sends a DRAW request carrying the raw pixel buffer at the given scale.
 *
 * Return: result of sys_conwi.
 */
int draw_buffer(uint32_t key, int x, int y, int width, int height, uint32_t scale, uint32_t *sprite) {
    window_t *entry = window_components[key];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

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

/**
 * register_section - allocate and register a new child window component.
 * @x: x position of the section, within the parent.
 * @y: y position of the section, within the parent.
 * @width: requested width.
 * @height: requested height.
 * @fg_color: foreground color.
 * @bg_color: background color.
 *
 * Description:
 * Finds a free slot in window_components, clamps the requested geometry
 * to fit inside the main window's drawable area, and allocates a new
 * window_t sharing the main window's window_id.
 *
 * Return: allocated slot index, or STATUS_ERROR.
 */
int register_section(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t fg_color, uint32_t bg_color) {

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

    memset(new_entry, 0, sizeof(window_t));
    uint32_t double_border = parent->border_width * 2;

    uint32_t max_w         = (parent->width > double_border) ? (parent->width - double_border) : 0;
    uint32_t max_h         = (parent->height > double_border) ? (parent->height - double_border) : 0;

    if (x < parent->border_width) {
        x = parent->border_width;
    }
    if (y < parent->border_width) {
        y = parent->border_width;
    }

    if (x >= parent->width - parent->border_width) {
        width = 0;
    } else if (x + width > parent->width - parent->border_width) {
        width = (parent->width - parent->border_width) - x;
    }

    if (y >= parent->height - parent->border_width) {
        height = 0;
    } else if (y + height > parent->height - parent->border_width) {
        height = (parent->height - parent->border_width) - y;
    }

    if (width > max_w)
        width = max_w;
    if (height > max_h)
        height = max_h;

    new_entry->window_id              = parent->window_id;
    new_entry->width                  = width;
    new_entry->height                 = height;
    new_entry->section_x_pos          = x;
    new_entry->section_y_pos          = y;
    new_entry->cursor_x_pos           = x;
    new_entry->cursor_y_pos           = y;
    new_entry->border_width           = 0;
    new_entry->def_horizontal_padding = parent->border_width + parent->def_horizontal_padding;
    new_entry->def_vertical_padding   = parent->border_width + parent->def_vertical_padding;
    new_entry->bg_color               = bg_color;
    new_entry->fg_color               = fg_color;
    new_entry->border_color           = COLOR_DARKER_GRAY;

    window_components[slot]           = new_entry;

    // paint_rectangle(x, y, width, height, background_color, slot);

    return slot;
}

/**
 * delete_section - Deallocate a section and null the entry slot.
 * @key: index of the section on the window components array.
 *
 * Description:
 * Validates the key and entry behind it. Then frees the heap memory.
 *
 * Return: STATUS_OK or STATUS_ERROR.
 */
int delete_section(uint32_t key) {
    if (key > MAX_SECTIONS) {
        LOG("Invalid key\n");
        return STATUS_ERROR;
    }

    window_t *entry = window_components[key];

    if (entry == NULL) {
        LOG("Invalid entry\n");
        return STATUS_ERROR;
    }

    free(entry);
    window_components[key] = NULL;

    return STATUS_OK;
}

/**
 * init_render - initialize the renderer and create the main window.
 *
 * Description:
 * Clears the window_components table, creates the main task window via
 * the kernel, allocates and populates its window_t metadata (colors,
 * border, padding, cursor start), then paints its initial border.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int init_render(void) {
    // LOG("Renderer initialized\n");
    memset(window_components, 0, sizeof(window_components));
    int window_id = create_task_window(10, 10, 300, 150, COLOR_WHITE, COLOR_BLACK);

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
    entry->border_color              = COLOR_ORAGNE_MUD;
    entry->border_width              = 4;
    entry->def_vertical_padding      = 1 + entry->border_width;
    entry->def_horizontal_padding    = 1 + entry->border_width;
    entry->width                     = 300;
    entry->height                    = 150;
    entry->cursor_x_pos              = entry->def_horizontal_padding;
    entry->cursor_y_pos              = entry->def_vertical_padding;
    entry->fg_color                  = COLOR_WHITE;
    entry->bg_color                  = COLOR_BLACK;

    window_components[RESERVED_SLOT] = entry;

    clamp_borders_to_window(MAIN_WINDOW_KEY);

    return STATUS_OK;
}
