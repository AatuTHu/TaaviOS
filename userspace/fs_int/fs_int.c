#include "folder.h"
#include "op_sy.h"
#include "stand.h"
#include "string.h"
#include <stddef.h>
#include <stdint.h>

#define BUF_SIZE 256

typedef void (*cmd_handler_t)(const char *arg);

typedef struct {
    const char *name;
    cmd_handler_t handler;
    int requires_arg;
} Command;

static int fd           = STATUS_ERROR;
static char dir_name[8] = {0};
static int is_running   = 1;

static void command_help(const char *arg) {
    (void)arg;
    print("\n------------------------------------------\n");
    print("Available commands\n");
    print("- help         'Prints this list'          \n");
    print("- open  [path] 'Open a file'               \n");
    print("- write [text] 'Writes to opened file'     \n");
    print("- mkdir [text] 'Creates a directory'       \n");
    print("- cd    [text] 'Changes working directory' \n");
    print("- ls           'Lists directory contents'  \n");
    print("- read         'Reads the opened file'     \n");
    print("- close        'Close opened file'         \n");
    print("- exit         'Close fs_interface'        \n");
}

static void command_write(const char *buf) {
    if (fd == STATUS_ERROR) {
        error("\nNo file currently open");
        return;
    }
    write(fd, buf);
}

static void command_open(const char *filename) {
    if (fd != STATUS_ERROR) {
        close(fd);
    }

    fd = open(filename, O_RDONLY | O_WRONLY);

    if (fd != STATUS_ERROR) {
        print("\nSuccesfully opened the file");
    } else {
        print("\n");
        error("Error on opening the file");
    }
}

static void command_read(const char *arg) {
    (void)arg;
    if (fd == STATUS_ERROR) {
        error("\nNo file currently open");
        return;
    }

    print("\n");
    char buf[512] = {0};
    int nread     = read(fd, buf, sizeof(buf) - 1);

    if (nread != STATUS_ERROR) {
        print(buf);
    } else {
        error("Read failed or file empty");
    }
}

static void command_close(const char *arg) {
    (void)arg;
    if (fd == STATUS_ERROR) {
        print("\nNo open file to close");
        return;
    }
    print("\nclosing file");
    close(fd);
    fd = STATUS_ERROR;
}

static void command_mkdir(const char *directory_name) {
    mkdir(directory_name);
    print("\n");
}

static void command_cd(const char *path) {
    if (change_directory(path, dir_name) == STATUS_ERROR) {
        error("Failed to change directory");
    }
}

static void command_ls(const char *arg) {
    (void)arg;
    char dirents[512] = {0};
    int read_size     = list_dirents(dirents, sizeof(dirents) - 1);
    if (read_size <= 0)
        return;

    print("\n");

    int current_y   = 10;
    int line_height = 12;
    char *entry     = dirents;

    for (int i = 0; i < read_size; i++) {
        if (dirents[i] == '\n' || dirents[i] == '\0') {
            dirents[i] = '\0';

            if (entry[0] != '\0') {
                // draw_buffer(32, 32, 10, current_y, 1, (uint32_t *)folder);
                // print("   ");
                print(entry);
                print("\n");
                current_y += line_height;
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
    is_running = 0;
}

static const Command commands[] = {
    {"help", command_help, 0},
    {"close", command_close, 0},
    {"read", command_read, 0},
    {"ls", command_ls, 0},
    {"exit", command_exit, 0},
    {"write ", command_write, 1},
    {"open ", command_open, 1},
    {"mkdir ", command_mkdir, 1},
    {"cd ", command_cd, 1},
};

void exec_cmd(char *buf) {
    size_t cmd_count = sizeof(commands) / sizeof(commands[0]);

    for (size_t i = 0; i < cmd_count; i++) {
        const Command *cmd = &commands[i];

        if (cmd->requires_arg) {
            if (str_starts_with(buf, cmd->name) == 1) {
                cmd->handler(buf + strlen(cmd->name));
                return;
            }
        } else {
            if (str_eq(buf, cmd->name) == 1) {
                cmd->handler(NULL);
                return;
            }
        }
    }

    print("\nInvalid command");
}

static void print_char(char c) {
    char tmp[2] = {c, '\0'};
    print(tmp);
}

int main(void) {
    if (create_task_window(800, 600, 20, 20) == STATUS_ERROR) {
        return 1;
    }

    print("filesystem interface\n");

    char buf[BUF_SIZE];
    int pos = 0;
    char c;

    while (is_running) {
        print("[ --> ");
        print(dir_name);
        print(" ] ");
        pos = 0;

        while (1) {
            if (scan(&c) <= 0) {
                continue;
            }

            if (c == '\n') {
                buf[pos] = '\0';
                print("\n");
                exec_cmd(buf);
                break;
            } else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    print("\b");
                }
            } else if (pos < BUF_SIZE - 1) {
                buf[pos++] = c;
                print_char(c);
            }
        }
    }

    return 0;
}
