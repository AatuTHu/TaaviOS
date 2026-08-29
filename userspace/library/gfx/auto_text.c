#include "font.h"
#include "log.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "sys_calls.h"
#include <stdint.h>

/**
 * TaaviOS auto_text.c
 * Design & Implementation:
 * @author: A.H, 2026
 */

/**
 * pack_params_and_send - build a gui_params_pack and send it to the kernel.
 * @opcode: operation to perform.
 * @region_id: target region id.
 * @x: x position.
 * @y: y position.
 * @buf: text buffer to send.
 * @fg: foreground color.
 * @bg: background color.
 *
 * Description:
 * This function centralizes the packing of gui_params_pack so callers don't
 * repeat the same memset/field assignment boilerplate.
 *
 * Return: result of sys_conwi.
 */
static int pack_params_and_send(uint8_t opcode, uint32_t region_id, uint32_t x, uint32_t y, const char *buf, uint32_t fg, uint32_t bg) {

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode      = opcode;
    params.struct_key  = region_id;
    params.x           = x;
    params.y           = y;
    params.buf         = (char *)buf;
    params.buffer_size = strlen(buf);
    params.fg_color    = fg;
    params.bg_color    = bg;

    return sys_conwi(&params);
}

/**
 * paint_cursor_position - repaint the cell at the primary viewport's cursor.
 * @color: color to paint the cursor cell.
 *
 * Description:
 * Used to draw cursor position vertical line next to chars
 * at the current cursor position with the given color.
 *
 * Return: void.
 */
void gfx_paint_cursor_position(uint32_t color) {
    gfx_region_t *entry = gfx_regions[PRIMARY_VIEWPORT_ID];
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode     = PAINT_WINDOW;
    params.struct_key = entry->id;
    params.x          = entry->cursor_x;
    params.y          = entry->cursor_y;
    params.width      = 1;
    params.height     = FONT_HEIGHT;
    params.bg_color   = color;
    params.fg_color   = entry->fg_color;
    sys_conwi(&params);
}

/**
 * gfx_draw_text_at - use when in need of a custom x and y positioning without x and y calculation
 * afterwards
 * @region_id: region component index.
 * @x: x position on the region.
 * @y: y position on the region.
 * @msg: printable text.
 *
 * Description:
 * This function prints text to the position given to it.
 * It does not parse or handle control chars and it uses colors from the components that the ID provides.
 *
 * Context: Function was made so that it would be easy to print chars to custom locations
 * within a single viewport without extra components.
 * Return: void.
 */
void gfx_draw_text_at(uint32_t region_id, uint32_t x, uint32_t y, const char *msg) {

    gfx_region_t *entry  = gfx_regions[region_id];
    gfx_region_t *parent = gfx_regions[PRIMARY_VIEWPORT_ID];

    if (x < parent->border_width) {
        x += parent->border_width;
    }

    if (y < parent->border_width) {
        y += parent->border_width;
    }

    pack_params_and_send(WRITE_AT, entry->id, x, y, msg, entry->fg_color, entry->bg_color);
}

/**
 * scroll_down - scroll a region's content up by one line.
 * @entry: region whose content should be scrolled.
 *
 * Description:
 * Sends a SCROLL_DOWN opcode for the given region using its current
 * background color and dimensions.
 *
 * Return: result of sys_conwi.
 */
static int scroll_down(gfx_region_t *entry) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode     = SCROLL_DOWN;
    params.struct_key = entry->id;
    params.bg_color   = entry->bg_color;
    params.height     = entry->height - entry->border_width * 2;
    params.width      = entry->width - entry->border_width * 2;
    params.x          = entry->offset_x + entry->border_width;
    params.y          = entry->offset_y + entry->border_width;
    return sys_conwi(&params);
}

/**
 * case_new_line - handle the \n control char during render.
 * @nwlind: starting index of the text.
 * @current_i: current index of text.
 * @text: text buffer being rendered.
 * @entry: entry whose metadata is used.
 *
 * Description:
 * This function handles the \n control char. First if there is anything to print it prints it,
 * then it calculates new cursor position and increments nwlind so that next part of the text starts
 * from there. After that it checks if the region has reached max Y and then scrolls the text one
 * line up so that next line is visible.
 *
 * Return: void.
 */
static void case_new_line(uint32_t *nwlind, uint32_t current_i, const char *text, gfx_region_t *entry) {
    uint32_t buffer_size = current_i - *nwlind;

    if (buffer_size > 0) {
        char temp_str[buffer_size + 1];
        memcpy(temp_str, &text[*nwlind], buffer_size);
        temp_str[buffer_size] = '\0';

        pack_params_and_send(WRITE_AT, entry->id, entry->cursor_x, entry->cursor_y,
                             temp_str, entry->fg_color, entry->bg_color);
    }

    entry->cursor_y += FONT_HEIGHT;
    entry->cursor_x = entry->padding_x;
    *nwlind         = current_i + 1;

    if ((entry->cursor_y + FONT_HEIGHT) >= (entry->height - entry->padding_y)) {
        scroll_down(entry);
        entry->cursor_y = entry->height - FONT_HEIGHT - entry->padding_y;
        entry->cursor_x = entry->padding_x;
    }
}

/**
 * backspace_pressed - sub function for gfx_draw_text.
 * @current_i: current index of text.
 * @entry: entry whose metadata is used.
 * @text: text buffer being rendered.
 * @new_line_ind: starting index of the text.
 *
 * Description:
 * First the function checks if the current index is higher than the new line index.
 * If it is, the function will calculate size of the text before it hit the backspace control char,
 * printing that part before actually doing the backspace operation. After that it moves cursor
 * back by one char and prints a space to that position.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
static int backspace_pressed(uint32_t current_i, gfx_region_t *entry, const char *text, uint32_t new_line_ind) {

    if (entry == NULL || text == NULL) {
        return STATUS_ERROR;
    }

    if (current_i > new_line_ind) {
        uint32_t buffer_size = current_i - new_line_ind;
        char temp_str[buffer_size + 1];
        memcpy(temp_str, &text[new_line_ind], buffer_size);
        temp_str[buffer_size] = '\0';
        pack_params_and_send(WRITE_AT, entry->id, entry->cursor_x,
                             entry->cursor_y, temp_str, entry->fg_color, entry->bg_color);
        entry->cursor_x += buffer_size * FONT_WIDTH;
    }

    if (entry->cursor_y == entry->padding_y && entry->cursor_x == entry->padding_x) {
        return STATUS_OK;
    }
    gfx_paint_cursor_position(entry->bg_color);

    if (entry->cursor_y >= (entry->padding_y + FONT_HEIGHT) && entry->cursor_x <= entry->padding_x) {
        entry->cursor_y -= FONT_HEIGHT;
        entry->cursor_x = entry->width - FONT_WIDTH - entry->padding_x;
    } else if (entry->cursor_x > entry->padding_x) {
        entry->cursor_x -= FONT_WIDTH;
    }

    pack_params_and_send(WRITE_AT, entry->id, entry->cursor_x, entry->cursor_y, " ", entry->fg_color, entry->bg_color);
    return STATUS_OK;
}

/**
 * gfx_draw_text - automatic text renderer.
 * @region_id: index to region component.
 * @text: text buffer to render.
 * @len: length of the text.
 *
 * Description:
 * This function provides automatic text handling for caller, printing it to correct position and
 * parsing the control characters. First it loops the entire text length to catch control chars.
 * If found it handles them accordingly and then moves the cursor position to right place.
 * After the loop it checks if there is anything left to print and prints the tail end of the text.
 *
 * Context: Designed to provide automatic cursor position and control char parsing for programs.
 * Return: STATUS_OK || STATUS_ERROR.
 */
int gfx_draw_text(uint32_t region_id, const char *text, uint32_t len) {
    uint32_t new_line_ind = 0;

    gfx_region_t *entry   = gfx_regions[region_id];
    gfx_region_t *parent  = gfx_regions[PRIMARY_VIEWPORT_ID];

    if (entry == NULL || parent == NULL) {
        return STATUS_ERROR;
    }

    for (uint32_t i = 0; i < len; i++) {

        switch (text[i]) {
        case '\n':
            case_new_line(&new_line_ind, i, text, entry);
            break;

        case '\b':
            backspace_pressed(i, entry, text, new_line_ind);
            new_line_ind = i + 1;
            break;
        case KEY_LEFT:
            if (entry->cursor_x > 0) {
                gfx_paint_cursor_position(entry->bg_color);
                entry->cursor_x -= FONT_WIDTH;
            }
            return STATUS_OK;
        case KEY_RIGHT:
            if (entry->cursor_x <= entry->width - FONT_WIDTH) {
                gfx_paint_cursor_position(entry->bg_color);
                entry->cursor_x += FONT_WIDTH;
            }
            return STATUS_OK;
        case KEY_UP:
            if (entry->cursor_y > 0) {
                gfx_paint_cursor_position(entry->bg_color);
                entry->cursor_y -= FONT_HEIGHT;
            }
            return STATUS_OK;
        case KEY_DOWN:
            if ((entry->cursor_y + FONT_HEIGHT) >= (entry->height - entry->padding_y)) {
                gfx_paint_cursor_position(entry->bg_color);
                scroll_down(entry);
                entry->cursor_y = entry->height - FONT_HEIGHT - entry->padding_y;
            } else {
                gfx_paint_cursor_position(entry->bg_color);
                entry->cursor_y += FONT_HEIGHT;
            }
            return STATUS_OK;
        }
        // gfx_clamp_horizontal(entry, parent);
        // gfx_clamp_vertical(entry, parent);
    }

    uint32_t buffer_size = len - new_line_ind;

    if (buffer_size > 0) {
        char temp_str[buffer_size + 1];
        memcpy(temp_str, &text[new_line_ind], buffer_size);
        temp_str[buffer_size] = '\0';

        pack_params_and_send(WRITE_AT, entry->id, entry->cursor_x,
                             entry->cursor_y, temp_str, entry->fg_color, entry->bg_color);

        entry->cursor_x += buffer_size * FONT_WIDTH;

        if (entry->cursor_x >= entry->width) {
            if ((entry->cursor_y + FONT_HEIGHT) >= (entry->height - entry->padding_y)) {
                scroll_down(entry);
                entry->cursor_y = entry->height - FONT_HEIGHT - entry->padding_y;
                entry->cursor_x = entry->padding_x;
                return STATUS_OK;
            }
            entry->cursor_y += FONT_HEIGHT;
            entry->cursor_x = entry->padding_x;
            //  gfx_clamp_horizontal(entry, parent);
            // gfx_clamp_vertical(entry, parent);
        }
    }
    return STATUS_OK;
}

/**
 * gfx_draw_text_to_region - automatic text renderer with custom component styling.
 * @region_id: component index.
 * @msg: printable text.
 *
 * Description:
 * This function prints text with specific component style. It calls gfx_draw_text inside.
 * 'gfx_draw_text' function then parses the msg for control chars and automatically updates cursor
 * position.
 *
 * Context: It was tiresome to manually change background color and foreground color.
 * Return: void.
 */
void gfx_draw_text_to_region(uint32_t region_id, const char *msg) {
    gfx_draw_text(region_id, msg, strlen(msg));
}
