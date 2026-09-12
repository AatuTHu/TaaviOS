#include "readline.h"
#include "font.h"
#include "history.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "ui.h"
#include <stdint.h>

static void clear_input_line(int current_pos, uint32_t region_id) {
    for (int i = 0; i < current_pos; i++) {
        print_to_region(region_id, "\b \b");
    }
}

static void rdl_handle(uint32_t region_id, char *buf, uint32_t buffer_size) {
    int pos         = 0;
    int history_idx = history_count();
    char c;

    while (1) {
        mark_cursor_position(COLOR_LIGHT_GRAY);
        scan(&c);

        switch (c) {
        case KEY_UP:
            if (history_count() > 0 && history_idx > 0 && history_idx > (history_count() - MAX_SAVED_LINES)) {
                history_idx--;
                const char *cmd = history_get(history_idx);
                if (cmd) {
                    clear_input_line(region_id, pos);
                    strncpy(buf, cmd, buffer_size - 1);
                    buf[buffer_size - 1] = '\0';
                    pos                  = strlen(buf);
                    print_to_region(region_id, buf);
                }
            }
            continue;

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
                        buf[buffer_size - 1] = '\0';
                        pos                  = strlen(buf);
                        print_to_region(region_id, buf);
                    }
                }
            }
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
    int pos         = 0;
    int history_idx = history_count();
    char c;

    while (1) {
        scan(&c);

        switch (c) {
        case KEY_UP:
            if (history_count() > 0 && history_idx > 0 && history_idx > (history_count() - MAX_SAVED_LINES)) {
                history_idx--;
                const char *cmd = history_get(history_idx);
                if (cmd) {
                    reset_region(region_id);
                    strncpy(buf, cmd, buffer_size - 1);
                    buf[buffer_size - 1] = '\0';
                    pos                  = strlen(buf);
                    print_at(region_id, x, y, buf);
                }
            }
            continue;

        case KEY_DOWN:
            if (history_idx < history_count()) {
                history_idx++;
                reset_region(region_id);

                if (history_idx == history_count()) {
                    buf[0] = '\0';
                    pos    = 0;
                } else {
                    const char *cmd = history_get(history_idx);
                    if (cmd) {
                        strncpy(buf, cmd, buffer_size - 1);
                        buf[buffer_size - 1] = '\0';
                        pos                  = strlen(buf);
                        print_at(region_id, x, y, buf);
                    }
                }
            }
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
