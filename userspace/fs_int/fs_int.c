#include "folder.h"
#include "font.h"
#include "op_sy.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include <stdint.h>

#define PADDING_BETWEEN_FILES 8
#define BUF_SIZE 256
#define WINDOW_WIDTH 600
#define WINDOW_HEIGTH 600
#define SCREEN_CO_X 20
#define SCREEN_CO_Y 20
#define PADDING 5
#define HEADER_Y 18
#define CMD_LINE_Y (WINDOW_HEIGTH - FONT_HEIGHT - PADDING)
#define INFO_LINE_Y (WINDOW_HEIGTH - (2 * FONT_HEIGHT) - PADDING)
typedef void (*cmd_handler_t)(const char *arg);

typedef struct {
    const char *name;
    cmd_handler_t handler;
    int requires_arg;
} Command;

static int fd           = -1;
static char dir_name[8] = {0};

static const char *skip_spaces(const char *str) {
    while (*str == ' ' || *str == '\t') {
        str++;
    }
    return str;
}

static void command_help(const char *arg) {
    (void)arg;
    print("------------------------------------------\n");
    print("Available commands:\n");
    print("- help         'Prints this list'\n");
    print("- open  [path] 'Open a file'\n");
    print("- write [text] 'Writes to opened file'\n");
    print("- mkdir [text] 'Creates a directory'\n");
    print("- cd    [text] 'Changes working directory'\n");
    print("- ls           'Lists directory contents'\n");
    print("- read         'Reads the opened file'\n");
    print("- close        'Close opened file'\n");
    print("- exit         'Close fs_interface'\n");
}

static void command_write(const char *buf) {
    if (fd == -1) {
        render_at(PADDING, INFO_LINE_Y, "No file currently open");
        return;
    }
    write(fd, buf);
}

static void command_open(const char *filename) {
    if (fd != -1) {
        close(fd);
    }

    fd = open(filename, O_RDONLY | O_WRONLY);

    if (fd == -1) {
        render_at(PADDING, INFO_LINE_Y, "read on opening the file");
        return;
    }
    render_at(PADDING, INFO_LINE_Y, "Successfully opened the file");
}

static void command_read(const char *arg) {
    (void)arg;
    if (fd == STATUS_ERROR) {
        render_at(PADDING, INFO_LINE_Y, "No file currently open");
        return;
    }

    char buf[512] = {0};
    int nread     = read(fd, buf, sizeof(buf) - 1);

    if (nread != -1) {
        print("\n\n");
        print(buf);
    } else {
        render_at(PADDING, CMD_LINE_Y, "Read failed or file empty");
    }
}

static void command_close(const char *arg) {
    (void)arg;
    if (fd == STATUS_ERROR) {
        render_at(PADDING, INFO_LINE_Y, "No open file to close");
        return;
    }
    render_at(PADDING, INFO_LINE_Y, "Closing file");
    close(fd);
    fd = -1;
}

static void command_mkdir(const char *directory_name) {
    mkdir(directory_name);
}

static void command_cd(const char *path) {
    if (change_directory(path, dir_name) == STATUS_ERROR) {
        render_at(PADDING, INFO_LINE_Y, "Failed to change directory");
    }
}

static void command_ls(const char *arg) {
    (void)arg;
    char dirents[512] = {0};
    int read_size     = list_dirents(dirents, sizeof(dirents) - 1);
    if (read_size <= 0) {
        return;
    }

    int current_x      = PADDING_BETWEEN_FILES;
    int current_text_y = 35 + HEADER_Y;
    int folder_y       = 10 + HEADER_Y;
    char *entry        = dirents;

    for (int i = 0; i < read_size; i++) {
        if (dirents[i] == '\n' || dirents[i] == '\0') {
            dirents[i] = '\0';

            trim(entry);

            if (entry[0] != '\0') {
                draw_buffer(32, 32, current_x, folder_y, 1, (uint32_t *)folder);
                render_at(current_x, current_text_y, entry);
                current_x += (strlen(entry) * FONT_WIDTH) + PADDING_BETWEEN_FILES;
            }
            entry = &dirents[i + 1];
        }
    }
}

static void command_exit(const char *arg) {
    (void)arg;
    if (fd != STATUS_ERROR) {
        close(fd);
        fd = STATUS_ERROR;
    }
    terminate_program();
}

static const Command commands[] = {
    {"help", command_help, 0},
    {"close", command_close, 0},
    {"read", command_read, 0},
    {"ls", command_ls, 0},
    {"exit", command_exit, 0},
    {"write", command_write, 1},
    {"open", command_open, 1},
    {"mkdir", command_mkdir, 1},
    {"cd", command_cd, 1},
};

void exec_cmd(char *buf) {
    set_background_color(COLOR_BLACK);

    const char *trimmed = skip_spaces(buf);
    if (*trimmed == '\0') {
        return;
    }

    size_t cmd_count = sizeof(commands) / sizeof(commands[0]);

    for (size_t i = 0; i < cmd_count; i++) {
        const Command *cmd = &commands[i];
        size_t len         = strlen(cmd->name);

        if (memcmp(trimmed, cmd->name, len) == 0) {
            char next = trimmed[len];
            if (next == ' ' || next == '\t' || next == '\0') {
                const char *arg = skip_spaces(trimmed + len);

                if (cmd->requires_arg && *arg == '\0') {
                    render_at(PADDING, INFO_LINE_Y, "Argument required");
                    return;
                }

                cmd->handler(arg);
                return;
            }
        }
    }

    render_at(PADDING, INFO_LINE_Y, "Invalid command");
}

static void print_char(int buffer_x_pos, char c) {
    char tmp[2] = {c, '\0'};
    render_at(buffer_x_pos, CMD_LINE_Y, tmp);
}

int main(void) {
    if (resize_task_window(WINDOW_WIDTH, WINDOW_HEIGTH) == STATUS_ERROR) {
        render_at(PADDING, INFO_LINE_Y, "Resizing window failed\n");
    }

    if (move_task_window(SCREEN_CO_X, SCREEN_CO_Y) == STATUS_ERROR) {
        render_at(PADDING, INFO_LINE_Y, "Moving task window failed\n");
    }

    set_text_color(COLOR_WHITE);

    if (paint_rectangle(WINDOW_WIDTH - (PADDING * 2), WINDOW_HEIGTH - (HEADER_Y + (FONT_HEIGHT * 3 + PADDING)), PADDING, HEADER_Y + (PADDING * 2), COLOR_GRAY) == STATUS_ERROR) {
        render_at(PADDING, INFO_LINE_Y, "Failed to paint main area rectange");
    }

    if (paint_rectangle(WINDOW_WIDTH - 2, (FONT_HEIGHT + 2), 1, CMD_LINE_Y - 1, COLOR_TEAL) == STATUS_ERROR) {
        set_text_color(COLOR_BLACK);
        render_at(PADDING, INFO_LINE_Y, "Failed to paint rectange to cmd line");
    }

    if (paint_rectangle(WINDOW_WIDTH - 1000, HEADER_Y + 2, 1, 1, COLOR_TEAL) == STATUS_ERROR) {
        render_at(PADDING, INFO_LINE_Y, "Failed to paint program header");
    }

    set_background_color(COLOR_TEAL);
    render_at(240, 3, "filesystem interface");
    char buf[BUF_SIZE];
    int pos = 0;
    char c;

    const char *art_start = "[ --> ";
    const char *art_end   = " ] ";

    while (1) {
        set_background_color(COLOR_TEAL);
        render_at(0, CMD_LINE_Y, "                                                                ");

        int buffer_x_pos = 5;
        render_at(buffer_x_pos, CMD_LINE_Y, art_start);
        buffer_x_pos += strlen(art_start) * FONT_WIDTH;

        render_at(buffer_x_pos, CMD_LINE_Y, dir_name);
        buffer_x_pos += strlen(dir_name) * FONT_WIDTH;

        render_at(buffer_x_pos, CMD_LINE_Y, art_end);
        buffer_x_pos += strlen(art_end) * FONT_WIDTH;

        pos = 0;

        while (1) {
            if (scan(&c) <= 0) {
                continue;
            }
            if (c == '\n') {
                buf[pos] = '\0';
                exec_cmd(buf);
                break;
            } else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    buffer_x_pos -= FONT_WIDTH;
                    render_at(buffer_x_pos, CMD_LINE_Y, " ");
                }
            } else if (pos < BUF_SIZE - 1) {
                buf[pos++] = c;
                print_char(buffer_x_pos, c);
                buffer_x_pos += FONT_WIDTH;
            }
        }
    }

    return 0;
}
