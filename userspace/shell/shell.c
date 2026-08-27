#include "malloc.h"
#include "op_sy.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include <stddef.h>
#include <stdint.h>

#define BUF_SIZE 256
#define MAX_SAVED_LINES 32

static char lines[MAX_SAVED_LINES][BUF_SIZE];
static int saved_cmds_count  = 0;
static int current_cmd_index = 0;

typedef void (*cmd_handler_t)(const char *arg);

typedef struct {
    const char *name;
    cmd_handler_t handler;
    int requires_arg;
} Command;

static void command_help(const char *arg) {
    (void)arg;
    print("----------------------------------------------------------------\n");
    print("Available commands                                              \n");
    print("- help         'Prints this list'                               \n");
    print("- get pid      'Shells pid'                                     \n");
    print("- exec [task]  'Executes a task'                                \n");
    print("- caw  [pid]   'Changes active window to provided pid'          \n");
    print("- kill [pid]   'Kills task with the corresponding pid'          \n");
    print("- move [x.y]   'Moves the window tho x and y location'          \n");
    print("- resize [x.y]   'Resizes the window to given width and height' \n");
    print("- tasks        'Lists active tasks with their pids'             \n");
    print("- exit         'Exits and kills shell'                          \n");
}

static void command_get_pid(const char *arg) {
    (void)arg;
    int pid = get_pid();
    if (pid < 0) {
        print("Failed to retrieve shell PID\n");
        return;
    }
    char msg[10];
    itoa(pid, msg);
    print("Shell PID: ");
    print(msg);
    print("\n");
}

static void command_exit(const char *arg) {
    (void)arg;
    terminate_program();
}

static void command_exec(const char *path) {
    if (path == NULL || *path == '\0') {
        print("Missing executable path\n");
        return;
    }
    if (exec(path) == STATUS_ERROR) {
        print("Failed to execute task\n");
    }
}

static void command_caw(const char *target) {
    if (target == NULL || *target == '\0') {
        print("Missing target PID\n");
        return;
    }
    int target_pid = atoi(target);
    if (target_pid <= 0 || ch_act_window(target_pid) == -1) {
        print("Window change failed\n");
    }
}

static void command_tasks(const char *arg) {
    (void)arg;
    char tasks[512] = {0};
    int result      = get_ac_tasks(tasks, sizeof(tasks));
    if (result > 0) {
        print(tasks);
    } else {
        print("Failed to retrieve task list\n");
    }
}

static void command_move(const char *positions) {
    if (!positions) {
        print("coo'ordinates were not valid\n");
        return;
    }

    const char *ptr = positions;

    while (*ptr == ' ') ptr++;

    if (*ptr < '0' || *ptr > '9') {
        print("separate x and y with a dot and try again\n");
        return;
    }

    int x = 0;
    while (*ptr >= '0' && *ptr <= '9') {
        x = x * 10 + (*ptr - '0');
        ptr++;
    }

    while (*ptr == ' ') ptr++;

    if (*ptr != '.') {
        print("separate x and y with a dot and try again\n");
        return;
    }
    ptr++;
    while (*ptr == ' ') ptr++;

    if (*ptr < '0' || *ptr > '9') {
        print("separate x and y with a dot and try again\n");
        return;
    }

    int y = 0;
    while (*ptr >= '0' && *ptr <= '9') {
        y = y * 10 + (*ptr - '0');
        ptr++;
    }

    if (move_viewport(x, y) == STATUS_ERROR) {
        print("Failed to move window\n");
    }
}

static void command_resize(const char *dimensions) {
    if (!dimensions) {
        print("coo'ordinates were not valid\n");
        return;
    }

    const char *ptr = dimensions;

    while (*ptr == ' ') ptr++;

    if (*ptr < '0' || *ptr > '9') {
        print("separate x and y with a dot and try again\n");
        return;
    }

    int width = 0;
    while (*ptr >= '0' && *ptr <= '9') {
        width = width * 10 + (*ptr - '0');
        ptr++;
    }

    while (*ptr == ' ') ptr++;

    if (*ptr != '.') {
        print("separate width and height with a dot and try again\n");
        return;
    }
    ptr++;
    while (*ptr == ' ') ptr++;

    if (*ptr < '0' || *ptr > '9') {
        print("separate x and y with a dot and try again\n");
        return;
    }

    int height = 0;
    while (*ptr >= '0' && *ptr <= '9') {
        height = height * 10 + (*ptr - '0');
        ptr++;
    }

    if (resize_viewport(width, height) == STATUS_ERROR) {
        print("Failed to resize window\n");
    }
}

static void command_kill(const char *target) {
    if (target == NULL || *target == '\0') {
        print("Missing target PID\n");
        return;
    }
    int target_pid = atoi(target);
    if (target_pid <= 0 || kill_task(target_pid) == -1) {
        print("Failed to kill task\n");
    }
}

static const Command commands[] = {
    {"help", command_help, 0},
    {"tasks", command_tasks, 0},
    {"get pid", command_get_pid, 0},
    {"exit", command_exit, 0},
    {"exec ", command_exec, 1},
    {"caw ", command_caw, 1},
    {"kill ", command_kill, 1},
    {"move ", command_move, 1},
    {"resize ", command_resize, 1},
};

static void save_cmd(const char *buf) {
    if (saved_cmds_count == MAX_SAVED_LINES) {
        saved_cmds_count = 0;
    }

    strcpy(lines[saved_cmds_count], buf);
    current_cmd_index = saved_cmds_count;
    saved_cmds_count++;
}

void exec_cmd(char *buf) {
    if (buf[0] == '\0') {
        return;
    }

    save_cmd(buf);

    size_t cmd_count = sizeof(commands) / sizeof(commands[0]);

    for (size_t i = 0; i < cmd_count; i++) {
        const Command *cmd = &commands[i];

        if (cmd->requires_arg) {
            if (str_starts_with(buf, cmd->name) == 1) {
                cmd->handler(buf + strlen(cmd->name));
                return;
            }
        } else {
            if (str_eq(buf, (char *)cmd->name) == 1) {
                cmd->handler(NULL);
                return;
            }
        }
    }

    print("Invalid command\n");
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

int main(void) {

    memset(lines, 0, sizeof(lines));

    if (set_operator_task() == -1) {
        print("Failed to set operator task\n");
        return 1;
    }

    if (resize_viewport(600, 200) == STATUS_ERROR) {
        print("Failed to resize window\n");
    }

    if (move_viewport(20, 750) == STATUS_ERROR) {
        print("Failed to move window\n");
    }

    print("TaaviOS - Operating shell\n");
    print("Type 'help' to see all commands\n");

    char buf[BUF_SIZE];
    int pos = 0;
    char c;

    while (1) {
        print("-> ");
        pos = 0;

        while (1) {
            //  paint_cursor_position(COLOR_LIGHT_GRAY);
            scan(&c);

            switch (c) {
            case KEY_LEFT:
                if (pos > 0) {
                    pos--;
                    print_char(c);
                }
                continue;
            case KEY_RIGHT:
                if (pos < BUF_SIZE - 1) {
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
                exec_cmd(buf);
                break;
            } else if (c == '\b') {
                if (pos > 0) {
                    pos--;
                    print_char(c);
                }
            } else if (pos < BUF_SIZE - 1) {
                buf[pos++] = c;
                print_char(c);
            }
        }
    }

    return 0;
}
