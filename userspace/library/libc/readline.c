#include "readline.h"
#include "font.h"
#include "history.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "ui.h"
#include <stdint.h>

int pos         = 0;
int history_idx = -1;

static void clear_input_line(uint32_t region_id, int current_pos) {
    for (int i = 0; i < current_pos; i++) {
        print_to_region(region_id, "\b \b");
    }
}

static int handle_hist_key(uint32_t region_id, const char c, char *buf, int buffer_size) {

    gfx_region_t *entry = gfx_regions[region_id];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

    switch (c) {
    case KEY_UP:
        if (history_count() > 0 && history_idx > 0 && history_idx > (history_count() - MAX_SAVED_LINES)) {
            history_idx--;
            const char *cmd = history_get(history_idx);
            if (cmd) {
                strncpy(buf, cmd, buffer_size - 1);
            }
        }
        break;
    case KEY_DOWN:
        if (history_idx < history_count()) {
            history_idx++;
            clear_input_line(region_id, pos);

            if (history_idx == history_count()) {
                buf[0] = '\0';
                pos    = 0;
            } else {
                const char *cmd = history_get(history_idx);
                if (cmd) {
                    strncpy(buf, cmd, buffer_size - 1);
                }
            }
        }
        break;

    default:
        return STATUS_ERROR;
    }

    return STATUS_OK;
}

static int handle_arrow_key(uint32_t region_id, const char c) {

    gfx_region_t *entry = gfx_regions[region_id];

    if (entry == NULL) {
        return STATUS_ERROR;
    }

    switch (c) {
    case KEY_LEFT:
        if (entry->cursor_x > entry->border_width + entry->padding_x) {
            mark_cursor_position(region_id, entry->bg_color);
            entry->cursor_x -= FONT_WIDTH;
        }
        break;
    case KEY_RIGHT:
        if (entry->cursor_x <= entry->width - FONT_WIDTH - entry->border_width - entry->padding_x) {
            mark_cursor_position(region_id, entry->bg_color);
            entry->cursor_x += FONT_WIDTH;
        }
        break;
    default:
        return STATUS_ERROR;
    }

    return STATUS_OK;
}

static void rdl_handle(uint32_t region_id, char *buf, uint32_t buffer_size) {
    pos         = 0;
    history_idx = history_count();
    char c;

    while (1) {
        mark_cursor_position(region_id, COLOR_LIGHT_GRAY);
        scan(&c);

        if (handle_hist_key(region_id, c, buf, buffer_size) == STATUS_OK) {
            clear_input_line(region_id, pos);
            buf[buffer_size - 1] = '\0';
            pos                  = strlen(buf);
            print_to_region(region_id, buf);
            continue;
        }

        if (handle_arrow_key(region_id, c) == STATUS_OK) {
            continue;
        }

        if (c == '\n') {
            buf[pos] = '\0';
            print_to_region(region_id, "\n");
            return;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                print_to_region(region_id, "\b \b");
            }
        } else if (pos < (int)buffer_size - 1) {
            buf[pos++]  = c;
            char tmp[2] = {c, '\0'};
            print_to_region(region_id, tmp);
        }
    }
}

void readline_at_region(uint32_t region_id, char *buf, uint32_t buffer_size) {
    rdl_handle(region_id, buf, buffer_size);
}

void readline(char *buf, uint32_t buffer_size) {
    rdl_handle(PRIMARY_VIEWPORT_ID, buf, buffer_size);
}

void readline_at(uint32_t region_id, char *buf, uint32_t buffer_size, uint32_t x, uint32_t y) {
    uint32_t start_x = x;
    char c;

    while (1) {
        scan(&c);

        if (handle_hist_key(region_id, c, buf, buffer_size) == STATUS_OK) {
            reset_region(region_id);
            buf[buffer_size - 1] = '\0';
            pos                  = strlen(buf);
            print_at(region_id, start_x, y, buf);
            x = start_x + (pos * FONT_WIDTH);
            continue;
        }

        if (handle_arrow_key(region_id, c) == STATUS_OK) {
            continue;
        }

        if (c == '\n') {
            buf[pos] = '\0';
            print_at(region_id, x, y, "\n");
            return;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                x -= FONT_WIDTH;
                print_at(region_id, x, y, " ");
            }
        } else if (pos < (int)buffer_size - 1) {
            buf[pos++]  = c;
            char tmp[2] = {c, '\0'};
            print_at(region_id, x, y, tmp);
            x += FONT_WIDTH;
        }
    }
}
