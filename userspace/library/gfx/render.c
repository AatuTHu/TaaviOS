#include "render.h"
#include "log.h"
#include "malloc.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "sys_calls.h"
#include <stdint.h>

gfx_region_t *gfx_regions[MAX_REGIONS];

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
 * gfx_draw_borders - repaint a region's border and interior.
 * @region_id: region whose border should be repainted.
 *
 * Description:
 * Paints the full region area with the border color, then paints the
 * inner area (inset by border_width on each side) with the background
 * color, producing a visible border frame.
 *
 * Return: void.
 */
static void gfx_draw_borders(uint32_t region_id) {
    gui_params_pack params;

    gfx_region_t *entry = gfx_regions[region_id];

    if (entry == NULL) {
        return;
    }

    memset(&params, 0, sizeof(params));
    params.opcode     = PAINT_WINDOW;
    params.struct_key = entry->id;

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
 * gfx_create_viewport - request a new top-level viewport from the kernel.
 * @x: x position of the viewport.
 * @y: y position of the viewport.
 * @w: width of the viewport.
 * @h: height of the viewport.
 * @fg_color: foreground color.
 * @bg_color: background color.
 *
 * Description:
 * Sends a CREATE request; the kernel allocates the viewport and returns
 * its ID. This ID is then passed back to kernel when making changes so that it can
 * correctly identify which viewport to modify.
 *
 * Return: new viewport ID, or STATUS_ERROR.
 */
int gfx_create_viewport(int x, int y, int w, int h, uint32_t fg_color, uint32_t bg_color) {

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode   = CREATE;
    params.width    = w;
    params.height   = h;
    params.x        = x;
    params.y        = y;
    params.fg_color = fg_color;
    params.bg_color = bg_color;

    return sys_conwi(&params);
}

/**
 * gfx_resize_viewport - request a resize of an existing viewport.
 * @region_id: region component to resize.
 * @w: requested width.
 * @h: requested height.
 *
 * Description:
 * Sends a RESIZE request and parses the kernel's confirmed "W . H"
 * response back into the component, then repaints its border to match.
 *
 * Kernel gives back new w and h because there is a chance it has to clamp
 * the requested paint inside the primary viewport.
 *
 * Return: result of sys_conwi.
 */
int gfx_resize_viewport(uint32_t region_id, int w, int h) {

    char dimensions_buffer[22];
    gfx_region_t *entry = gfx_regions[region_id];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode      = RESIZE;
    params.struct_key  = entry->id;
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

    gfx_draw_borders(region_id);

    return result;
}

/**
 * gfx_clear_region - repaint a region using its own metadata.
 * @region_id: region component to repaint.
 *
 * Description:
 * Paints the component's full area at its current position using
 * its own background and foreground colors.
 *
 * Return: result of sys_conwi.
 */
int gfx_clear_region(uint32_t region_id) {
    gfx_region_t *entry = gfx_regions[region_id];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

    gui_params_pack params;
    memset(&params, 0, sizeof(params));

    params.opcode     = PAINT_WINDOW;
    params.struct_key = entry->id;
    params.width      = entry->width;
    params.height     = entry->height;
    params.x          = entry->offset_x;
    params.y          = entry->offset_y;
    params.bg_color   = entry->bg_color;
    params.fg_color   = entry->fg_color;

    return sys_conwi(&params);
}

/**
 * gfx_fill_rect - paint a color-filled rectangle within the primary viewport.
 * @x: x position of the rectangle.
 * @y: y position of the rectangle.
 * @width: requested width.
 * @height: requested height.
 * @color: fill color.
 *
 * Description:
 * Clamps the requested rectangle to stay inside the primary viewport's border,
 * shrinking width/height (or zeroing them) if the position or size would
 * otherwise overflow the drawable area.
 *
 * Return: result of sys_conwi, or STATUS_ERROR if there is no primary viewport.
 */
int gfx_fill_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t color) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    gfx_region_t *entry = gfx_regions[PRIMARY_VIEWPORT_ID];

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
    params.struct_key = entry->id;
    params.width      = width;
    params.height     = height;
    params.x          = x;
    params.y          = y;
    params.bg_color   = color;
    params.fg_color   = entry->fg_color;

    return sys_conwi(&params);
}

/**
 * gfx_move_viewport - request a move of an existing viewport.
 * @region_id: region component to move.
 * @x: new x position.
 * @y: new y position.
 *
 * Description:
 * Sends a MOVE request using the component's own colors.
 *
 * Return: result of sys_conwi.
 */
int gfx_move_viewport(uint32_t region_id, int x, int y) {
    gfx_region_t *entry = gfx_regions[region_id];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode     = MOVE;
    params.struct_key = entry->id;
    params.x          = x;
    params.y          = y;
    params.fg_color   = entry->fg_color;
    params.bg_color   = entry->bg_color;

    return sys_conwi(&params);
}

/**
 * gfx_draw_sprite - blit a pixel buffer into a region.
 * @region_id: region component to draw into.
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
int gfx_draw_sprite(uint32_t region_id, int x, int y, int width, int height, uint32_t scale, uint32_t *sprite) {
    gfx_region_t *entry = gfx_regions[region_id];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode     = DRAW;
    params.struct_key = entry->id;
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
 * gfx_register_region - allocate and register a new child region.
 * @x: x position of the region, within the parent.
 * @y: y position of the region, within the parent.
 * @width: requested width.
 * @height: requested height.
 * @fg_color: foreground color.
 * @bg_color: background color.
 *
 * Description:
 * Finds a free slot in gfx_regions, clamps the requested geometry
 * to fit inside the primary viewport's drawable area, and allocates a new
 * gfx_region_t sharing the primary viewport's id.
 *
 * Return: allocated slot index, or STATUS_ERROR.
 */
int gfx_register_region(uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t fg_color, uint32_t bg_color) {

    int slot = -1;

    for (int i = RESERVED_REGION_ID; i < MAX_REGIONS; i++) {
        if (gfx_regions[i] == NULL) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        LOG("Could not find free slot for region\n");
        return STATUS_ERROR;
    }

    gfx_region_t *parent = gfx_regions[PRIMARY_VIEWPORT_ID];

    if (parent == NULL) {
        LOG("Cant register a region if there is no parent viewport\n");
        return STATUS_ERROR;
    }

    gfx_region_t *new_entry = (gfx_region_t *)malloc(sizeof(gfx_region_t));

    if (new_entry == NULL) {
        LOG("Could not allocate memory for new region entry\n");
        return STATUS_ERROR;
    }

    memset(new_entry, 0, sizeof(gfx_region_t));
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

    new_entry->id           = parent->id;
    new_entry->width        = width;
    new_entry->height       = height;
    new_entry->offset_x     = x;
    new_entry->offset_y     = y;
    new_entry->cursor_x     = x;
    new_entry->cursor_y     = y;
    new_entry->border_width = 0;
    new_entry->padding_x    = parent->border_width + parent->padding_x;
    new_entry->padding_y    = parent->border_width + parent->padding_y;
    new_entry->bg_color     = bg_color;
    new_entry->fg_color     = fg_color;
    new_entry->border_color = COLOR_DARKER_GRAY;

    gfx_regions[slot]       = new_entry;

    gfx_clear_region(slot);

    return slot;
}

/**
 * gfx_delete_region - Deallocate a region and null the entry slot.
 * @region_id: index of the region on the gfx_regions array.
 *
 * Description:
 * Validates the ID and entry behind it. Then frees the heap memory.
 *
 * Return: STATUS_OK or STATUS_ERROR.
 */
int gfx_delete_region(uint32_t region_id) {
    if (region_id >= MAX_REGIONS) {
        LOG("Invalid region_id\n");
        return STATUS_ERROR;
    }

    gfx_region_t *entry = gfx_regions[region_id];

    if (entry == NULL) {
        LOG("Invalid entry\n");
        return STATUS_ERROR;
    }

    free(entry);
    gfx_regions[region_id] = NULL;

    return STATUS_OK;
}

/**
 * gfx_init - initialize the renderer and create the primary viewport.
 *
 * Description:
 * Clears the gfx_regions table, creates the primary viewport via
 * the kernel, allocates and populates its gfx_region_t metadata (colors,
 * border, padding, cursor start), then paints its initial border.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int gfx_init(void) {
    memset(gfx_regions, 0, sizeof(gfx_regions));
    int id = gfx_create_viewport(10, 10, 300, 150, COLOR_WHITE, COLOR_BLACK);

    if (id == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    gfx_region_t *entry = (gfx_region_t *)malloc(sizeof(gfx_region_t));

    if (entry == NULL) {
        LOG("Cant allocate entry for the primary viewport\n");
        return STATUS_ERROR;
    }

    entry->id                        = id;
    entry->border_color              = COLOR_ORAGNE_MUD;
    entry->border_width              = 4;
    entry->padding_y                 = 1 + entry->border_width;
    entry->padding_x                 = 1 + entry->border_width;
    entry->width                     = 300;
    entry->height                    = 150;
    entry->cursor_x                  = entry->padding_x;
    entry->cursor_y                  = entry->padding_y;
    entry->fg_color                  = COLOR_WHITE;
    entry->bg_color                  = COLOR_BLACK;

    gfx_regions[PRIMARY_VIEWPORT_ID] = entry;

    gfx_draw_borders(PRIMARY_VIEWPORT_ID);

    return STATUS_OK;
}

int gfx_reset_cursor(uint32_t region_id) {
    if (region_id >= MAX_REGIONS) {
        LOG("Invalid region_id\n");
        return STATUS_ERROR;
    }

    gfx_region_t *entry = gfx_regions[region_id];

    if (entry == NULL) {
        LOG("Invalid entry\n");
        return STATUS_ERROR;
    }

    entry->cursor_x = entry->offset_x;
    entry->cursor_y = entry->offset_y;

    return STATUS_OK;
}
