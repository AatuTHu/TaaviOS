#include "render.h"
#include "font.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "sys_calls.h"
#include <stdint.h>

/**
 * TaaviOS window manager
 * Design & Implementation:
 * @author: A.H, 2026
 */

static int def_horizontal_padding = 5;
static int def_vertical_padding   = 5;
static int width                  = 0;
static int height                 = 0;
static int x_pos                  = 0;
static int y_pos                  = 0;
static int fg_color               = 0;
static int bg_color               = 0;

void render_at(uint32_t x, uint32_t y, const char *msg) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode      = WRITE_AT;
    params.x           = x;
    params.y           = y;
    params.buf         = (char *)msg;
    params.buffer_size = strlen(msg);
    params.bg_color    = bg_color;
    params.fg_color    = fg_color;
    sys_conwi(&params);
}

static int scroll_down() {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode   = SCROLL_DOWN;
    params.fg_color = fg_color;
    params.bg_color = bg_color;
    return sys_conwi(&params);
}

static void case_new_line(uint32_t *nwlind, uint32_t current_i, const char *text) {
    uint32_t buffer_size = current_i - *nwlind;

    if (buffer_size > 0) {
        char temp_str[buffer_size + 1];
        memcpy(temp_str, &text[*nwlind], buffer_size);
        temp_str[buffer_size] = '\0';

        gui_params_pack params;
        memset(&params, 0, sizeof(params));
        params.opcode      = WRITE_AT;
        params.x           = x_pos;
        params.y           = y_pos;
        params.buf         = temp_str;
        params.buffer_size = buffer_size;
        params.fg_color    = fg_color;
        params.bg_color    = bg_color;
        sys_conwi(&params);
    }

    y_pos += FONT_HEIGHT;
    x_pos   = def_horizontal_padding;
    *nwlind = current_i + 1;
}

static int backspace_pressed() {
    if (y_pos == def_vertical_padding && x_pos == def_horizontal_padding) {
        return STATUS_OK;
    }

    if (y_pos >= (def_vertical_padding + FONT_HEIGHT) && x_pos <= def_horizontal_padding) {
        y_pos -= FONT_HEIGHT;
        x_pos = width - FONT_WIDTH - def_horizontal_padding;
    } else if (x_pos > def_horizontal_padding) {
        x_pos -= FONT_WIDTH;
    }

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode      = WRITE_AT;
    params.x           = x_pos;
    params.y           = y_pos;
    params.buf         = " ";
    params.buffer_size = 1;
    params.fg_color    = fg_color;
    params.bg_color    = bg_color;
    sys_conwi(&params);
    return STATUS_OK;
}

int render(const char *text, uint32_t len) {
    uint32_t new_line_ind = 0;

    for (uint32_t i = 0; i < len; i++) {
        switch (text[i]) {
        case '\n':
            case_new_line(&new_line_ind, i, text);
            if ((y_pos + FONT_HEIGHT) >= (height - def_vertical_padding)) {
                scroll_down();
                y_pos = height - FONT_HEIGHT - def_vertical_padding;
                x_pos = def_horizontal_padding;
            }
            break;

        case '\b':
            if (i > new_line_ind) {
                uint32_t buffer_size = i - new_line_ind;
                char temp_str[buffer_size + 1];
                memcpy(temp_str, &text[new_line_ind], buffer_size);
                temp_str[buffer_size] = '\0';

                gui_params_pack params;
                memset(&params, 0, sizeof(params));
                params.opcode      = WRITE_AT;
                params.x           = x_pos;
                params.y           = y_pos;
                params.buf         = temp_str;
                params.buffer_size = buffer_size;
                params.fg_color    = fg_color;
                params.bg_color    = bg_color;
                sys_conwi(&params);
                x_pos += buffer_size * FONT_WIDTH;
            }
            backspace_pressed();
            new_line_ind = i + 1;
            break;
        }
    }

    uint32_t buffer_size = len - new_line_ind;

    if (buffer_size > 0) {
        char temp_str[buffer_size + 1];
        memcpy(temp_str, &text[new_line_ind], buffer_size);
        temp_str[buffer_size] = '\0';

        gui_params_pack params;
        memset(&params, 0, sizeof(params));
        params.opcode      = WRITE_AT;
        params.x           = x_pos;
        params.y           = y_pos;
        params.buf         = temp_str;
        params.buffer_size = buffer_size;
        params.fg_color    = fg_color;
        params.bg_color    = bg_color;
        sys_conwi(&params);

        x_pos += buffer_size * FONT_WIDTH;

        if (x_pos >= width) {
            if ((y_pos + FONT_HEIGHT) >= (height - def_vertical_padding)) {
                scroll_down();
                y_pos = height - FONT_HEIGHT - def_vertical_padding;
                x_pos = def_horizontal_padding;
                return STATUS_OK;
            }
            y_pos += FONT_HEIGHT;
            x_pos = def_horizontal_padding;
        }
    }
    return STATUS_OK;
}

int create_task_window(int w, int h, int x, int y) {

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

int resize_task_window(int w, int h) {

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode   = RESIZE;
    params.width    = w;
    params.height   = h;
    params.fg_color = fg_color;
    params.bg_color = bg_color;
    int result      = sys_conwi(&params);

    if (result == -1) {
        return result;
    }

    width  = w;
    height = h;
    return result;
}

int paint_rectangle(uint32_t width, uint32_t height, uint32_t x, uint32_t y, uint32_t color) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode   = PAINT_WINDOW;
    params.width    = width;
    params.height   = height;
    params.x        = x;
    params.y        = y;
    params.bg_color = color;
    params.fg_color = fg_color;

    return sys_conwi(&params);
}

int move_task_window(int x, int y) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode   = MOVE;
    params.x        = x;
    params.y        = y;
    params.fg_color = fg_color;
    params.bg_color = bg_color;
    return sys_conwi(&params);
}

int draw_buffer(int width, int height, int x, int y, uint32_t scale, uint32_t *sprite) {
    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode   = DRAW;
    params.width    = width;
    params.height   = height;
    params.x        = x;
    params.y        = y;
    params.scale    = scale;
    params.pixels   = sprite;
    params.fg_color = fg_color;
    params.bg_color = bg_color;
    return sys_conwi(&params);
}

void set_background_color(uint32_t color) {
    bg_color = color;
}

void set_text_color(uint32_t color) {
    fg_color = color;
}

void set_dimensions(uint32_t w, uint32_t h) {
    width  = w;
    height = h;
}

void set_vertical_padding(uint32_t vp) {
    def_vertical_padding = vp;
}

void set_horizontal_padding(uint32_t hp) {
    def_horizontal_padding = hp;
}

int init_render() {
    def_vertical_padding   = 1;
    def_horizontal_padding = 1;
    width                  = 200;
    height                 = 100;
    x_pos                  = def_horizontal_padding;
    y_pos                  = def_vertical_padding;
    fg_color               = COLOR_WHITE;
    bg_color               = COLOR_BLACK;

    return 0;
}
