#include "readline.h"
#include "font.h"
#include "history.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "ui.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

typedef struct {
    uint32_t region_id;
    uint32_t pos;
    uint32_t buffer_size;
    uint32_t x;
    uint32_t y;
    int history_idx;
    bool is_absolute;
} rdl_obj_t;

static void clear_input_line(rdl_obj_t *rdl_obj) {

    if (rdl_obj->is_absolute) {
        reset_region(rdl_obj->region_id);
        return;
    }

    for (uint32_t i = 0; i < rdl_obj->pos; i++) {
        print_to_region(rdl_obj->region_id, "\b \b");
    }
}

static inline void handle_backspace(rdl_obj_t *rdl_obj) {

    if (rdl_obj == NULL || rdl_obj->pos <= 0) {
        return;
    }

    rdl_obj->pos--;

    if (rdl_obj->is_absolute) {
        rdl_obj->x -= FONT_WIDTH;
        print_at(rdl_obj->region_id, rdl_obj->x, rdl_obj->y, " ");
        return;
    }

    gfx_region_t *region = gfx_regions[rdl_obj->region_id];

    if (region != NULL) {
        mark_cursor_position(rdl_obj->region_id, region->bg_color);
    }

    print_to_region(rdl_obj->region_id, "\b \b");
}

static int handle_hist_key(rdl_obj_t *rdl_obj, const char c, char *buf) {

    switch (c) {
    case KEY_UP:
        if (history_count() > 0 && rdl_obj->history_idx > 0 && rdl_obj->history_idx > (history_count() - MAX_SAVED_LINES)) {
            rdl_obj->history_idx--;
            const char *cmd = history_get(rdl_obj->history_idx);
            if (cmd) {
                strncpy(buf, cmd, rdl_obj->buffer_size - 1);
            }
        }
        break;
    case KEY_DOWN:
        if (rdl_obj->history_idx < history_count()) {
            rdl_obj->history_idx++;
            clear_input_line(rdl_obj);

            if (rdl_obj->history_idx == history_count()) {
                buf[0]       = '\0';
                rdl_obj->pos = 0;
            } else {
                const char *cmd = history_get(rdl_obj->history_idx);
                if (cmd) {
                    strncpy(buf, cmd, rdl_obj->buffer_size - 1);
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

static void rdl_handle(rdl_obj_t *rdl_obj, char *buf) {

    char c;

    while (1) {
        mark_cursor_position(rdl_obj->region_id, COLOR_LIGHT_GRAY);
        scan(&c);

        int starting_buffer_len = strlen(buf);
        if (handle_hist_key(rdl_obj, c, buf) == STATUS_OK) {
            if (starting_buffer_len < strlen(buf)) {
                clear_input_line(rdl_obj);
                buf[rdl_obj->buffer_size - 1] = '\0';
                rdl_obj->pos                  = strlen(buf);

                if (rdl_obj->is_absolute) {
                    print_at(rdl_obj->region_id, rdl_obj->x, rdl_obj->y, buf);
                } else {
                    print_to_region(rdl_obj->region_id, buf);
                }
            }
            continue;
        }

        if (handle_arrow_key(rdl_obj->region_id, c) == STATUS_OK) {
            continue;
        }

        if (c == '\n') {
            buf[rdl_obj->pos] = '\0';
            if (rdl_obj->is_absolute) {
                print_at(rdl_obj->region_id, rdl_obj->x, rdl_obj->y, "\n");
            } else {
                print_to_region(rdl_obj->region_id, "\n");
            }

            return;
        }
        if (c == '\b') {
            handle_backspace(rdl_obj);
            continue;
        }

        if (rdl_obj->pos < rdl_obj->buffer_size - 1) {
            buf[rdl_obj->pos++] = c;
            char tmp[2]         = {c, '\0'};

            if (rdl_obj->is_absolute) {
                print_at(rdl_obj->region_id, rdl_obj->x, rdl_obj->y, tmp);
                rdl_obj->x += FONT_WIDTH;
            } else {
                print_to_region(rdl_obj->region_id, tmp);
            }
        }
    }
}

void readline_at_region(uint32_t region_id, char *buf, uint32_t buffer_size) {
    rdl_obj_t rdl_obj;
    memset(&rdl_obj, 0, sizeof(rdl_obj));

    rdl_obj.region_id   = region_id;
    rdl_obj.buffer_size = buffer_size;
    rdl_obj.pos         = 0;
    rdl_obj.history_idx = history_count();
    rdl_obj.is_absolute = false;

    rdl_handle(&rdl_obj, buf);
}

void readline(char *buf, uint32_t buffer_size) {
    rdl_obj_t rdl_obj;
    memset(&rdl_obj, 0, sizeof(rdl_obj));

    rdl_obj.region_id   = PRIMARY_VIEWPORT_ID;
    rdl_obj.buffer_size = buffer_size;
    rdl_obj.pos         = 0;
    rdl_obj.history_idx = history_count();
    rdl_obj.is_absolute = false;
    rdl_handle(&rdl_obj, buf);
}

void readline_at(uint32_t region_id, char *buf, uint32_t buffer_size, uint32_t x, uint32_t y) {

    rdl_obj_t rdl_obj;
    memset(&rdl_obj, 0, sizeof(rdl_obj));

    rdl_obj.region_id   = region_id;
    rdl_obj.buffer_size = buffer_size;
    rdl_obj.pos         = 0;
    rdl_obj.x           = x;
    rdl_obj.y           = y;
    rdl_obj.history_idx = history_count();
    rdl_obj.is_absolute = true;

    rdl_handle(&rdl_obj, buf);
}
