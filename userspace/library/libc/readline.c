#include "readline.h"
#include "shared.h"
#include "stand.h"
#include "stdbool.h"
#include "string.h"
#include "ui.h"
#include <stdint.h>

#define MAX_SAVED_LINES 32

static char lines[MAX_SAVED_LINES][512];
static int saved_cmds_count  = 0;
static int current_cmd_index = 0;
static bool has_initialized  = false;

static void save_cmd(const char *buf) {
    if (saved_cmds_count == MAX_SAVED_LINES) {
        saved_cmds_count = 0;
    }

    strcpy(lines[saved_cmds_count], buf);
    current_cmd_index = saved_cmds_count;
    saved_cmds_count++;
}

static void print_char(char c) {
    char tmp[2] = {c, '\0'};
    print(tmp);
}

static void clear_input_line(int current_pos) {
    for (int i = 0; i < current_pos; i++) {
        print("\b \b");
    }
}

void readline(char *buf, uint32_t buffer_size) {

    if (!has_initialized) {
        memset(lines, 0, sizeof(lines));
        has_initialized = true;
    }

    int pos = 0;
    char c;

    while (1) {
        mark_cursor_position(COLOR_LIGHT_GRAY);
        scan(&c);

        switch (c) {
        case KEY_LEFT:
            if (pos > 0) {
                pos--;
                print_char(c);
            }
            continue;
        case KEY_RIGHT:
            if (pos < (int)buffer_size - 1) {
                pos++;
                print_char(c);
            }
            continue;
        case KEY_UP:
            if (saved_cmds_count > 0) {
                clear_input_line(pos);
                int len = strlen(lines[current_cmd_index]);
                strcpy(buf, lines[current_cmd_index]);
                pos = len;
                print(buf);

                if (current_cmd_index <= 0) {
                    current_cmd_index = saved_cmds_count;
                    continue;
                }

                current_cmd_index--;
            }

            continue;
        case KEY_DOWN:
            clear_input_line(pos);
            continue;
        }

        if (c == '\n') {
            buf[pos] = '\0';
            print_char(c);
            save_cmd(buf);
            return;
        } else if (c == '\b') {
            if (pos > 0) {
                pos--;
                print_char(c);
            }
        } else if (pos < (int)buffer_size - 1) {
            buf[pos++] = c;
            print_char(c);
        }
    }
}
