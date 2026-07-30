#include "render.h"
#include "font.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "sys_calls.h"
#include <stdint.h>

static int def_horizontal_padding = 5;
static int def_vertical_padding   = 5;
static int width                  = 0;
static int height                 = 0;
static int x_pos                  = 0;
static int y_pos                  = 0;

static int scroll_down() {
    int status = sys_conwi(SCROLL_DOWN, 0, 0, 0, y_pos);

    return status;
}

static void case_new_line(uint32_t *nwlind, uint32_t current_i, const char *text) {
    uint32_t buffer_size = current_i - *nwlind;

    if (buffer_size > 0) {
        char temp_str[buffer_size + 1];
        memcpy(temp_str, &text[*nwlind], buffer_size);
        temp_str[buffer_size] = '\0';
        sys_wriat(WRITE_AT, x_pos, y_pos, buffer_size, temp_str);
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

    sys_wriat(WRITE_AT, x_pos, y_pos, 1, " ");
    return STATUS_OK;
}

int render(const char *text, uint32_t len) {

    uint32_t new_line_ind = 0;

    for (uint32_t i = 0; i < len; i++) {
        if (text[i] == '\0') {
            break;
        }

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
                sys_wriat(WRITE_AT, x_pos, y_pos, buffer_size, temp_str);
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
        sys_wriat(WRITE_AT, x_pos, y_pos, buffer_size, temp_str);
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

void init_renderer(uint32_t w, uint32_t h) {
    def_vertical_padding   = 1;
    def_horizontal_padding = 1;
    width                  = w;
    height                 = h;
    x_pos                  = def_horizontal_padding;
    y_pos                  = def_vertical_padding;
}
