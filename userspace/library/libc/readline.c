#include "readline.h"
#include "history.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "ui.h"
#include <stdint.h>

static void clear_input_line(int current_pos) {
    for (int i = 0; i < current_pos; i++) {
        print("\b \b");
    }
}

void readline(char *buf, uint32_t buffer_size) {
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
                    clear_input_line(pos);
                    strncpy(buf, cmd, buffer_size - 1);
                    buf[buffer_size - 1] = '\0';
                    pos                  = strlen(buf);
                    print(buf);
                }
            }
            continue;

        case KEY_DOWN:
            if (history_idx < history_count()) {
                history_idx++;
                clear_input_line(pos);

                if (history_idx == history_count()) {
                    buf[0] = '\0';
                    pos    = 0;
                } else {
                    const char *cmd = history_get(history_idx);
                    if (cmd) {
                        strncpy(buf, cmd, buffer_size - 1);
                        buf[buffer_size - 1] = '\0';
                        pos                  = strlen(buf);
                        print(buf);
                    }
                }
            }
            continue;
        }

        if (c == '\n') {
            buf[pos] = '\0';
            print("\n");
            return;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                print("\b \b");
            }
        } else if (pos < (int)buffer_size - 1) {
            buf[pos++]  = c;
            char tmp[2] = {c, '\0'};
            print(tmp);
        }
    }
}
