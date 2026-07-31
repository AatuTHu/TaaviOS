#include "op_sy.h"
#include "render.h"
#include "shared.h"
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

static void command_help(const char *arg) {
    (void)arg;
    print("------------------------------------------------------------\n");
    print("Available commands                                          \n");
    print("- help         'Prints this list'                           \n");
    print("- get pid      'Shells pid'                                 \n");
    print("- exec [task]  'Executes a task'                            \n");
    print("- caw  [pid]   'Changes active window to provided pid'      \n");
    print("- kill [pid]   'Kills task with the corresponding pid'      \n");
    print("- tasks        'Lists active tasks with their pids'         \n");
    print("- exit         'Exits and kills shell'                      \n");
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
        print("\n");
    } else {
        print("Failed to retrieve task list\n");
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
};

void exec_cmd(char *buf) {
    if (buf[0] == '\0') {
        return;
    }

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

    print("Invalid command\n");
}

static void print_char(char c) {
    char tmp[2] = {c, '\0'};
    print(tmp);
}

int main(void) {
    if (set_operator_task() == -1) {
        print("Failed to set operator task\n");
        return 1;
    }

    if (create_task_window(600, 200, 20, 780) == STATUS_ERROR) {
        print("Failed to create task window\n");
        return 1;
    }

    set_text_color(COLOR_WHITE);
    print("TaaviOS - Operating shell\n");
    print("Type 'help' to see all commands\n");

    char buf[BUF_SIZE];
    int pos = 0;
    char c;

    while (1) {
        print("-> ");
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
