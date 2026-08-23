#include "font.h"
#include "log.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "sys_calls.h"
#include <stdint.h>

/**
 * TaaviOS printing manager
 * Design & Implementation:
 * @author: A.H, 2026
 */

/**
 * pack_params_and_send - build a gui_params_pack and send it to the kernel.
 * @opcode: operation to perform.
 * @key: target window id.
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
static int pack_params_and_send(uint8_t opcode, uint32_t key, uint32_t x, uint32_t y, const char *buf, uint32_t fg, uint32_t bg) {

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode      = opcode;
    params.struct_key  = key;
    params.x           = x;
    params.y           = y;
    params.buf         = (char *)buf;
    params.buffer_size = strlen(buf);
    params.fg_color    = fg;
    params.bg_color    = bg;

    return sys_conwi(&params);
}

/**
 * render_at_section - print to the part of the window within a custom component.
 * @section_key: component key.
 * @x: x position on the component.
 * @y: y position on the component.
 * @msg: printable text.
 *
 * Description:
 * This function prints text with specific component style. Using its colors
 *
 * Context: It was tiresome to manually change background color and foreground color.
 * Return: void.
 */
void render_at_section(uint32_t section_key, uint32_t x, uint32_t y, const char *msg) {

    window_t *entry  = window_components[section_key];
    window_t *parent = window_components[MAIN_WINDOW_KEY];

    if (entry == NULL || parent == NULL) {
        LOG("Invalid entry\n");
        return;
    }

    if (x < parent->border_width) {
        x += parent->border_width;
    }

    if (y < parent->border_width) {
        y += parent->border_width;
    }
    pack_params_and_send(WRITE_AT, entry->window_id, x, y, msg, entry->fg_color, entry->bg_color);
}

/**
 * paint_cursor_position - repaint the cell at the main window's cursor.
 * @color: color to paint the cursor cell.
 *
 * Description:
 * Used to draw cursor position vertical line next to chars
 * at the current cursor position with the given color.
 *
 * Return: void.
 */
void paint_cursor_position(uint32_t color) {
    window_t *entry = window_components[MAIN_WINDOW_KEY];
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode     = PAINT_WINDOW;
    params.struct_key = entry->window_id;
    params.x          = entry->cursor_x_pos;
    params.y          = entry->cursor_y_pos;
    params.width      = 1;
    params.height     = FONT_HEIGHT;
    params.bg_color   = color;
    params.fg_color   = entry->fg_color;
    sys_conwi(&params);
}

/**
 * render_at - use when in need of a custom x and y positioning
 * @x: x position on the window.
 * @y: y position on the window.
 * @msg: printable text.
 *
 * Description:
 * This function prints text to the position given to it.
 * It does not parse or handle control chars and it uses main window metadata.
 *
 * Context: Function was made so that it would be easy to print chars to custom locations
 * withing a singe window without extra components.
 * Return: void.
 */
void render_at(uint32_t x, uint32_t y, const char *msg) {

    window_t *entry = window_components[MAIN_WINDOW_KEY];

    if (x < entry->border_width) {
        x += entry->border_width;
    }

    if (y < entry->border_width) {
        y += entry->border_width;
    }

    pack_params_and_send(WRITE_AT, entry->window_id, x, y, msg, entry->fg_color, entry->bg_color);
}

/**
 * scroll_down - scroll a window's content up by one line.
 * @entry: window whose content should be scrolled.
 *
 * Description:
 * Sends a SCROLL_DOWN opcode for the given window using its current
 * background color and dimensions.
 *
 * Return: result of sys_conwi.
 */
static int scroll_down(window_t *entry) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode     = SCROLL_DOWN;
    params.struct_key = entry->window_id;
    params.bg_color   = entry->bg_color;
    params.height     = entry->height;
    params.width      = entry->width;
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
 * This function handles the \n control char. First if the is anything to print it prints it.
 * then it calculates new cursor position and increments nwlind so that next part of the text start
 * from there. After that it checks if the windows has reached max Y and then scrolls the text one
 * line up so that next line is visible.
 *
 * Return: void.
 */
static void case_new_line(uint32_t *nwlind, uint32_t current_i, const char *text, window_t *entry) {
    uint32_t buffer_size = current_i - *nwlind;

    if (buffer_size > 0) {
        char temp_str[buffer_size + 1];
        memcpy(temp_str, &text[*nwlind], buffer_size);
        temp_str[buffer_size] = '\0';

        pack_params_and_send(WRITE_AT, entry->window_id, entry->cursor_x_pos, entry->cursor_y_pos,
                             temp_str, entry->fg_color, entry->bg_color);
    }

    entry->cursor_y_pos += FONT_HEIGHT;
    entry->cursor_x_pos = entry->def_horizontal_padding;
    *nwlind             = current_i + 1;

    if ((entry->cursor_y_pos + FONT_HEIGHT) >= (entry->height - entry->def_vertical_padding)) {
        scroll_down(entry);
        entry->cursor_y_pos = entry->height - FONT_HEIGHT - entry->def_vertical_padding;
        entry->cursor_x_pos = entry->def_horizontal_padding;
    }
}

/**
 * backspace_pressed - sub function for render.
 * @current_i: current index of text.
 * @entry: entry whos metadata is used.
 * @text: text buffer being rendered.
 * @new_line_ind: starting index of the text
 *
 * Description:
 * First the function check if the current index is higher than the new line index.
 * If it is the function will calculate size of the text before it hit the backspace control char
 * Printing that part before actually doing the backspace operation. After that it moves cursor
 * back by one char and prints a space to that position.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
static int backspace_pressed(uint32_t current_i, window_t *entry, const char *text, uint32_t new_line_ind) {

    if (entry == NULL || text == NULL) {
        return STATUS_ERROR;
    }

    if (current_i > new_line_ind) {
        uint32_t buffer_size = current_i - new_line_ind;
        char temp_str[buffer_size + 1];
        memcpy(temp_str, &text[new_line_ind], buffer_size);
        temp_str[buffer_size] = '\0';
        pack_params_and_send(WRITE_AT, entry->window_id, entry->cursor_x_pos,
                             entry->cursor_y_pos, temp_str, entry->fg_color, entry->bg_color);
        entry->cursor_x_pos += buffer_size * FONT_WIDTH;
    }

    if (entry->cursor_y_pos == entry->def_vertical_padding && entry->cursor_x_pos == entry->def_horizontal_padding) {
        return STATUS_OK;
    }

    if (entry->cursor_y_pos >= (entry->def_vertical_padding + FONT_HEIGHT) && entry->cursor_x_pos <= entry->def_horizontal_padding) {
        entry->cursor_y_pos -= FONT_HEIGHT;
        entry->cursor_x_pos = entry->width - FONT_WIDTH - entry->def_horizontal_padding;
    } else if (entry->cursor_x_pos > entry->def_horizontal_padding) {
        entry->cursor_x_pos -= FONT_WIDTH;
    }

    pack_params_and_send(WRITE_AT, entry->window_id, entry->cursor_x_pos, entry->cursor_y_pos, " ", entry->fg_color, entry->bg_color);
    return STATUS_OK;
}

/**
 * render - automatic text renderer.
 * @text: text buffer to render.
 * @len: length of the text.
 *
 * Description:
 * This function provides automatic text handling for caller, printing it to correct position and
 * parsing the control characters. First it loops the entire text lenght to catch control chars.
 * If found it handles them accordingly and then moves the cursor position to right place.
 * After the loop it checks if there is anything left to print and prints the tail end of the text.
 *
 * Context: I wanted to provide automatic cursor position and control char parsing for the programs.
 * Return: STATUS_OK || STATUS_ERROR.
 */
int render(const char *text, uint32_t len) {
    uint32_t new_line_ind = 0;

    window_t *entry       = window_components[MAIN_WINDOW_KEY];

    if (entry == NULL) {
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
            if (entry->cursor_x_pos > 0) {
                entry->cursor_x_pos -= FONT_WIDTH;
                paint_cursor_position(entry->bg_color);
            }
            return STATUS_OK;
        case KEY_RIGHT:
            if (entry->cursor_x_pos <= entry->width - FONT_WIDTH) {
                paint_cursor_position(entry->bg_color);
                entry->cursor_x_pos += FONT_WIDTH;
            }
            return STATUS_OK;
        case KEY_UP:
            if (entry->cursor_y_pos > 0) {
                entry->cursor_y_pos -= FONT_HEIGHT;
                paint_cursor_position(entry->bg_color);
            }
            return STATUS_OK;
        case KEY_DOWN:
            if ((entry->cursor_y_pos + FONT_HEIGHT) >= (entry->height - entry->def_vertical_padding)) {
                scroll_down(entry);
                entry->cursor_y_pos = entry->height - FONT_HEIGHT - entry->def_vertical_padding;
            } else {
                entry->cursor_y_pos += FONT_HEIGHT;
            }
            return STATUS_OK;
        }
    }

    uint32_t buffer_size = len - new_line_ind;

    if (buffer_size > 0) {
        char temp_str[buffer_size + 1];
        memcpy(temp_str, &text[new_line_ind], buffer_size);
        temp_str[buffer_size] = '\0';

        pack_params_and_send(WRITE_AT, entry->window_id, entry->cursor_x_pos,
                             entry->cursor_y_pos, temp_str, entry->fg_color, entry->bg_color);

        entry->cursor_x_pos += buffer_size * FONT_WIDTH;

        if (entry->cursor_x_pos >= entry->width) {
            if ((entry->cursor_y_pos + FONT_HEIGHT) >= (entry->height - entry->def_vertical_padding)) {
                scroll_down(entry);
                entry->cursor_y_pos = entry->height - FONT_HEIGHT - entry->def_vertical_padding;
                entry->cursor_x_pos = entry->def_horizontal_padding;
                return STATUS_OK;
            }
            entry->cursor_y_pos += FONT_HEIGHT;
            entry->cursor_x_pos = entry->def_horizontal_padding;
        }
    }
    return STATUS_OK;
}
