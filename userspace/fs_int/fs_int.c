#include "document.h"
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
#define PADDING 2

#define CMD_LINE_Y WINDOW_HEIGTH - FONT_HEIGHT - PADDING * 2
#define INFO_LINE_Y CMD_LINE_Y - FONT_HEIGHT
#define MAIN_AREA_START_Y HEADER_BLOCK

#define HEADER_BLOCK FONT_HEIGHT + PADDING
#define FOOTER_BLOCK FONT_HEIGHT * 2 + PADDING * 2
#define MAIN_AREA_BLOCK WINDOW_HEIGTH - FOOTER_BLOCK - HEADER_BLOCK

typedef void (*cmd_handler_t)(const char *arg);

static int header_id = -1;
static int main_id   = -1;
static int footer_id = -1;

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

static void show_commands(const char *args) {
    (void)args;
    render_at_section(PADDING, MAIN_AREA_START_Y + PADDING, "Available commands:", main_id);
    render_at_section(PADDING, MAIN_AREA_START_Y + PADDING + FONT_HEIGHT, "- help         'Prints this list'", main_id);
    render_at_section(PADDING, MAIN_AREA_START_Y + PADDING + FONT_HEIGHT * 2, "- open  [path] 'Open a file'", main_id);
    render_at_section(PADDING, MAIN_AREA_START_Y + PADDING + FONT_HEIGHT * 3, "- write [text] 'Writes to opened file'", main_id);
    render_at_section(PADDING, MAIN_AREA_START_Y + PADDING + FONT_HEIGHT * 4, "- mkdir [text] 'Creates a directory'", main_id);
    render_at_section(PADDING, MAIN_AREA_START_Y + PADDING + FONT_HEIGHT * 5, "- cd    [text] 'Changes working directory'", main_id);
    render_at_section(PADDING, MAIN_AREA_START_Y + PADDING + FONT_HEIGHT * 6, "- ls           'Lists directory contents'", main_id);
    render_at_section(PADDING, MAIN_AREA_START_Y + PADDING + FONT_HEIGHT * 7, "- read         'Reads the opened file'", main_id);
    render_at_section(PADDING, MAIN_AREA_START_Y + PADDING + FONT_HEIGHT * 8, "- close        'Close opened file'", main_id);
    render_at_section(PADDING, MAIN_AREA_START_Y + PADDING + FONT_HEIGHT * 9, "- exit         'Close fs_interface'", main_id);
}

static void close_file(const char *args) {
    (void)args;
    if (fd == STATUS_ERROR) {
        render_at_section(PADDING, INFO_LINE_Y, "No open file to close", footer_id);
        return;
    }
    close(fd);
    render_at_section(PADDING, INFO_LINE_Y, "File closed", footer_id);
    fd = -1;
}

static void list_directories(const char *args) {
    (void)args;
    char dirents[512] = {0};
    int read_size     = list_dirents(dirents, sizeof(dirents) - 1);
    if (read_size <= 0) {
        return;
    }

    int current_x      = PADDING_BETWEEN_FILES;
    int current_text_y = 35 + HEADER_BLOCK;
    int folder_y       = 10 + HEADER_BLOCK;
    char *entry        = dirents;
    for (int i = 0; i < read_size; i++) {
        if (dirents[i] == '\n' || dirents[i] == '\0') {
            dirents[i] = '\0';

            trim(entry);

            if (entry[0] != '\0') {
                if (strlen(entry) > 8) {
                    draw_buffer(32, 32, current_x, folder_y, 1, (uint32_t *)document, main_id);
                } else {
                    draw_buffer(32, 32, current_x, folder_y, 1, (uint32_t *)folder, main_id);
                }
                render_at_section(current_x, current_text_y, entry, main_id);
                current_x += (strlen(entry) * FONT_WIDTH) + PADDING_BETWEEN_FILES;
            }
            entry = &dirents[i + 1];
        }
    }
}

static void quit_program(const char *args) {
    (void)args;
    if (fd != STATUS_ERROR) {
        close(fd);
        fd = STATUS_ERROR;
    }
    terminate_program();
}

static void read_file(const char *args) {
    (void)args;
    if (fd == STATUS_ERROR) {
        render_at_section(PADDING, INFO_LINE_Y, "No file currently open", footer_id);
        return;
    }

    char buf[512] = {0};
    int nread     = read(fd, buf, sizeof(buf) - 1);

    if (nread != -1) {
        render_at_section(PADDING, MAIN_AREA_START_Y, buf, main_id);
    } else {
        render_at_section(PADDING, CMD_LINE_Y, "Read failed or file empty", footer_id);
    }
}

static void write_to_file(const char *buffer) {
    if (fd == -1) {
        render_at_section(PADDING, INFO_LINE_Y, "No file currently open", footer_id);
        return;
    }
    write(fd, buffer);
    render_at_section(PADDING, INFO_LINE_Y, "Wrote to the file, now reading it", footer_id);
    read_file(0);
}
static void open_file(const char *flag_and_path) {
    if (fd != -1) {
        close(fd);
    }

    char *cpy_path = (char *)flag_and_path;
    uint32_t flag  = O_RDONLY;

    if (str_starts_with(cpy_path, "-a ")) {
        flag = O_APPEND;
        cpy_path += 3;
    } else if (str_starts_with(cpy_path, "-r ")) {
        flag = O_RDONLY;
        cpy_path += 3;
    } else if (str_starts_with(cpy_path, "-w ")) {
        flag = O_WRONLY;
        cpy_path += 3;
    } else if (str_starts_with(cpy_path, "-rw ")) {
        flag = O_RDWR;
        cpy_path += 4;
    } else {
        render_at_section(PADDING, INFO_LINE_Y, "special flag not provided opening with read_only", footer_id);
        cpy_path += 3;
    }

    fd = open(cpy_path, flag);
    if (fd == -1) {
        render_at_section(PADDING, INFO_LINE_Y, "read on opening the file", footer_id);
        return;
    }
    render_at_section(PADDING, INFO_LINE_Y, "Opened the file, now reading it", footer_id);
    read_file(0);
}

static void create_dir(const char *flag_and_path) {
    if (mkdir(flag_and_path) == STATUS_ERROR) {
        render_at_section(PADDING, INFO_LINE_Y, "Could not create directories", footer_id);
        return;
    }
    render_at_section(PADDING, INFO_LINE_Y, "Directory(ies) created", footer_id);
}

static void change_dir(const char *path) {
    if (change_directory(path, dir_name) == STATUS_ERROR) {
        render_at_section(PADDING, INFO_LINE_Y, "Failed to change directory", footer_id);
    }
}

static const Command commands[] = {
    {"help", show_commands, 0},
    {"close", close_file, 0},
    {"ls", list_directories, 0},
    {"exit", quit_program, 0},
    {"read", read_file, 0},
    {"write", write_to_file, 1},
    {"open", open_file, 1},
    {"mkdir", create_dir, 1},
    {"cd", change_dir, 1},
};

void exec_cmd(char *buf) {
    paint_section(main_id);

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
                    return;
                }

                cmd->handler(arg);
                return;
            }
        }
    }
}

static void print_char(int buffer_x_pos, char c) {
    char tmp[2] = {c, '\0'};
    render_at_section(buffer_x_pos, CMD_LINE_Y, tmp, footer_id);
}

int main(void) {
    set_text_color(COLOR_WHITE, MAIN_WINDOW_KEY);
    if (resize_task_window(WINDOW_WIDTH, WINDOW_HEIGTH, MAIN_WINDOW_KEY) == STATUS_ERROR) {
    }

    if (move_task_window(SCREEN_CO_X, SCREEN_CO_Y, MAIN_WINDOW_KEY) == STATUS_ERROR) {
    }

    header_id = register_section(WINDOW_WIDTH, HEADER_BLOCK, 0, 0, COLOR_BLACK, COLOR_LIGHT_GRAY);

    if (header_id != STATUS_ERROR) {
        paint_section(header_id);
        render_at_section(PADDING, PADDING, "Maccas filesystem interface", header_id);
    }

    main_id = register_section(WINDOW_WIDTH, MAIN_AREA_BLOCK, 0, MAIN_AREA_START_Y,
                               COLOR_WHITE, COLOR_DEEP_BLUE);

    if (main_id != STATUS_ERROR) {
        paint_section(main_id);
        show_commands(0);
    }

    footer_id = register_section(WINDOW_WIDTH, FOOTER_BLOCK, 0, INFO_LINE_Y, COLOR_BLACK, COLOR_LIGHT_GRAY);

    if (footer_id != STATUS_ERROR) {
        paint_section(footer_id);
    }

    char buf[BUF_SIZE];
    int pos = 0;
    char c;

    const char *art_start = "[ --> ";
    const char *art_end   = " ] ";

    render_at_section(PADDING, INFO_LINE_Y, "Maccas configurated and ready for use", footer_id);

    while (1) {
        paint_section(footer_id);
        int buffer_x_pos = PADDING;
        render_at_section(buffer_x_pos, CMD_LINE_Y, art_start, footer_id);
        buffer_x_pos += strlen(art_start) * FONT_WIDTH;

        render_at_section(buffer_x_pos, CMD_LINE_Y, dir_name, footer_id);
        buffer_x_pos += strlen(dir_name) * FONT_WIDTH;

        render_at_section(buffer_x_pos, CMD_LINE_Y, art_end, footer_id);
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
                    render_at_section(buffer_x_pos, CMD_LINE_Y, " ", footer_id);
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
