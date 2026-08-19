#include "font.h"
#include "log.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "sys_calls.h"
#include <stdint.h>

void render_at_section(uint32_t x, uint32_t y, const char *msg, uint32_t section_key) {

    window_t *entry = window_components[section_key];

    if (entry == NULL) {
        LOG("Invalid entry\n");
        return;
    }

    if (x < entry->border_width) {
        x += entry->border_width;
    }

    if (y < entry->border_width) {
        y += entry->border_width;
    }

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode      = WRITE_AT;
    params.struct_key  = entry->window_id;
    params.x           = x;
    params.y           = y;
    params.buf         = (char *)msg;
    params.buffer_size = strlen(msg);
    params.bg_color    = entry->bg_color;
    params.fg_color    = entry->fg_color;
    sys_conwi(&params);
}

void render_at(uint32_t x, uint32_t y, const char *msg) {

    window_t *entry = window_components[MAIN_WINDOW_KEY];

    if (x < entry->border_width) {
        x += entry->border_width;
    }

    if (y < entry->border_width) {
        y += entry->border_width;
    }

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode      = WRITE_AT;
    params.struct_key  = entry->window_id;
    params.x           = x;
    params.y           = y;
    params.buf         = (char *)msg;
    params.buffer_size = strlen(msg);
    params.bg_color    = entry->bg_color;
    params.fg_color    = entry->fg_color;
    sys_conwi(&params);
}

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

static void case_new_line(uint32_t *nwlind, uint32_t current_i, const char *text, window_t *entry) {
    uint32_t buffer_size = current_i - *nwlind;

    if (buffer_size > 0) {
        char temp_str[buffer_size + 1];
        memcpy(temp_str, &text[*nwlind], buffer_size);
        temp_str[buffer_size] = '\0';

        gui_params_pack params;
        memset(&params, 0, sizeof(params));
        params.opcode      = WRITE_AT;
        params.struct_key  = entry->window_id;
        params.x           = entry->cursor_x_pos;
        params.y           = entry->cursor_y_pos;
        params.buf         = temp_str;
        params.buffer_size = buffer_size;
        params.fg_color    = entry->fg_color;
        params.bg_color    = entry->bg_color;
        sys_conwi(&params);
    }

    entry->cursor_y_pos += FONT_HEIGHT;
    entry->cursor_x_pos = entry->def_horizontal_padding;
    *nwlind             = current_i + 1;
}

static int backspace_pressed(window_t *entry) {
    if (entry->cursor_y_pos == entry->def_vertical_padding && entry->cursor_x_pos == entry->def_horizontal_padding) {
        return STATUS_OK;
    }

    if (entry->cursor_y_pos >= (entry->def_vertical_padding + FONT_HEIGHT) && entry->cursor_x_pos <= entry->def_horizontal_padding) {
        entry->cursor_y_pos -= FONT_HEIGHT;
        entry->cursor_x_pos = entry->width - FONT_WIDTH - entry->def_horizontal_padding;
    } else if (entry->cursor_x_pos > entry->def_horizontal_padding) {
        entry->cursor_x_pos -= FONT_WIDTH;
    }

    gui_params_pack params;
    memset(&params, 0, sizeof(params));
    params.opcode      = WRITE_AT;
    params.struct_key  = entry->window_id;
    params.x           = entry->cursor_x_pos;
    params.y           = entry->cursor_y_pos;
    params.buf         = " ";
    params.buffer_size = 1;
    params.fg_color    = entry->fg_color;
    params.bg_color    = entry->bg_color;
    sys_conwi(&params);
    return STATUS_OK;
}

int render(const char *text, uint32_t len) {
    uint32_t new_line_ind = 0;

    window_t *entry       = window_components[MAIN_WINDOW_KEY];

    for (uint32_t i = 0; i < len; i++) {
        switch (text[i]) {
        case '\n':
            case_new_line(&new_line_ind, i, text, entry);
            if ((entry->cursor_y_pos + FONT_HEIGHT) >= (entry->height - entry->def_vertical_padding)) {
                scroll_down(entry);
                entry->cursor_y_pos = entry->height - FONT_HEIGHT - entry->def_vertical_padding;
                entry->cursor_x_pos = entry->def_horizontal_padding;
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
                params.struct_key  = entry->window_id;
                params.x           = entry->cursor_x_pos;
                params.y           = entry->cursor_y_pos;
                params.buf         = temp_str;
                params.buffer_size = buffer_size;
                params.fg_color    = entry->fg_color;
                params.bg_color    = entry->bg_color;
                sys_conwi(&params);
                entry->cursor_x_pos += buffer_size * FONT_WIDTH;
            }
            backspace_pressed(entry);
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
        params.struct_key  = entry->window_id;
        params.x           = entry->cursor_x_pos;
        params.y           = entry->cursor_y_pos;
        params.buf         = temp_str;
        params.buffer_size = buffer_size;
        params.fg_color    = entry->fg_color;
        params.bg_color    = entry->bg_color;
        sys_conwi(&params);

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
