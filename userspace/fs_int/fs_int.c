#include "document.h"
#include "folder.h"
#include "font.h"
#include "op_sy.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include "ui.h"
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
static int cmd_id    = -1;
static int info_id   = -1;

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
    print_to_region(main_id, "Available commands:\n");
    print_to_region(main_id, "- help          'Prints this list'\n");
    print_to_region(main_id, "- open   [path] 'Open a file'\n");
    print_to_region(main_id, "- write  [text] 'Writes to opened file'\n");
    print_to_region(main_id, "- mkdir  [path] 'Creates a directory'\n");
    print_to_region(main_id, "- delete [path] 'Deletes a file\n'");
    print_to_region(main_id, "- cd     [path] 'Changes working directory'\n");
    print_to_region(main_id, "- ls            'Lists directory contents'\n");
    print_to_region(main_id, "- read          'Reads the opened file'\n");
    print_to_region(main_id, "- close         'Close opened file'\n");
    print_to_region(main_id, "- exit          'Close fs_interface'\n");
}

static void delete(const char *path) {
    if (delete_file(path) == STATUS_ERROR) {
        print_at(info_id, PADDING, INFO_LINE_Y, "Failed to delete file!");
        return;
    }

    print_at(info_id, PADDING, INFO_LINE_Y, "File deleted!");
}

static void close_file(const char *args) {
    (void)args;
    if (fd == STATUS_ERROR) {
        print_at(info_id, PADDING, INFO_LINE_Y, "No open file to close");
        return;
    }
    close(fd);
    print_at(info_id, PADDING, INFO_LINE_Y, "File closed");
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
                    draw_sprite(main_id, current_x, folder_y, 32, 32, 1, (uint32_t *)document);
                } else {
                    draw_sprite(main_id, current_x, folder_y, 32, 32, 1, (uint32_t *)folder);
                }
                print_at(main_id, current_x, current_text_y, entry);
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
        print_at(info_id, PADDING, INFO_LINE_Y, "No file currently open");
        return;
    }

    char buf[512] = {0};
    int nread     = read(fd, buf, sizeof(buf) - 1);

    if (nread != -1) {
        print_to_region(main_id, buf);
    } else {
        print_at(info_id, PADDING, INFO_LINE_Y, "Read failed or file empty");
    }
}

static void write_to_file(const char *buffer) {
    if (fd == -1) {
        print_at(info_id, PADDING, INFO_LINE_Y, "No file currently open");
        return;
    }
    write(fd, buffer);
    print_at(info_id, PADDING, INFO_LINE_Y, "Wrote to the file, now reading it");
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
    } else if (str_starts_with(cpy_path, "-c ")) {
        flag = O_CREAT;
        cpy_path += 3;
    } else {
        print_at(info_id, PADDING, INFO_LINE_Y, "special flag not provided opening with read_only");
    }

    fd = open(cpy_path, flag);
    if (fd == -1) {
        print_at(info_id, PADDING, INFO_LINE_Y, "read on opening the file");
        return;
    }
    print_at(info_id, PADDING, INFO_LINE_Y, "Opened the file, now reading it");
    read_file(0);
}

static void create_dir(const char *flag_and_path) {
    if (mkdir(flag_and_path) == STATUS_ERROR) {
        print_at(info_id, PADDING, INFO_LINE_Y, "Could not create directories");
        return;
    }
    print_to_region(info_id, "Directory(ies) created");
}
static void change_dir(const char *path) {
    if (change_directory(path, dir_name) == STATUS_ERROR) {
        print_at(info_id, PADDING, INFO_LINE_Y, "Failed to change directory");
    }
    list_directories(0);
}

static const Command commands[] = {
    {"help", show_commands, 0},
    {"ls", list_directories, 0},
    {"close", close_file, 0},
    {"exit", quit_program, 0},
    {"read", read_file, 0},
    {"write", write_to_file, 1},
    {"open", open_file, 1},
    {"mkdir", create_dir, 1},
    {"cd", change_dir, 1},
    {"delete", delete, 1},
};

void exec_cmd(char *buf) {
    refresh_region(cmd_id);
    refresh_region(main_id);

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
    print_at(cmd_id, buffer_x_pos, CMD_LINE_Y, tmp);
}

int main(void) {
    set_viewport_text_color(COLOR_WHITE);
    set_viewport_background_color(COLOR_DEEP_BLUE);
    if (resize_viewport(WINDOW_WIDTH, WINDOW_HEIGTH) == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    if (move_viewport(SCREEN_CO_X, SCREEN_CO_Y) == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    header_id = create_container(WINDOW_WIDTH, HEADER_BLOCK, 0, 0, COLOR_BLACK, COLOR_LIGHT_GRAY);

    if (header_id == STATUS_ERROR) {
        return STATUS_ERROR;
    }
    print_at(header_id, PADDING, 0, "Maccas filesystem interface");

    main_id = create_container(WINDOW_WIDTH - 2, MAIN_AREA_BLOCK - FONT_HEIGHT, 0, MAIN_AREA_START_Y,
                               COLOR_WHITE, COLOR_DEEP_BLUE);

    if (main_id == STATUS_ERROR) {
        return STATUS_ERROR;
    }
    set_region_padding_x(main_id, PADDING);
    show_commands(0);

    info_id = create_container(WINDOW_WIDTH, FONT_HEIGHT + PADDING, 0, INFO_LINE_Y, COLOR_BLACK, COLOR_LIGHT_GRAY);

    if (info_id == STATUS_ERROR) {
        return STATUS_ERROR;
    }
    set_region_padding_x(info_id, PADDING);

    cmd_id = create_container(WINDOW_WIDTH, FONT_HEIGHT + PADDING, 0, CMD_LINE_Y, COLOR_BLACK, COLOR_LIGHT_GRAY);
    if (cmd_id == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    set_region_padding_x(cmd_id, PADDING);
    char buf[BUF_SIZE];
    int pos = 0;
    char c;

    const char *art_start = "[ --> ";
    const char *art_end   = " ] ";

    print_at(info_id, PADDING, INFO_LINE_Y, "Maccas configurated and ready for use");

    while (1) {
        int buffer_x_pos = PADDING;
        print_at(cmd_id, buffer_x_pos, CMD_LINE_Y, art_start);
        buffer_x_pos += strlen(art_start) * FONT_WIDTH;

        print_at(cmd_id, buffer_x_pos, CMD_LINE_Y, dir_name);
        buffer_x_pos += strlen(dir_name) * FONT_WIDTH;

        print_at(cmd_id, buffer_x_pos, CMD_LINE_Y, art_end);
        buffer_x_pos += strlen(art_end) * FONT_WIDTH;

        pos = 0;

        while (1) {
            scan(&c);

            if (c == '\n') {
                buf[pos] = '\0';
                exec_cmd(buf);
                break;
            } else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    buffer_x_pos -= FONT_WIDTH;
                    print_at(cmd_id, buffer_x_pos, CMD_LINE_Y, " ");
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
