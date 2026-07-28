#include "op_sy.h"
#include "stand.h"
#include "string.h"
#include <stddef.h>
#include <stdint.h>

static void command_help() {
    print("\n------------------------------------------------------------\n");
    print("Available commands                                            \n");
    print("- help                'Prints this list'                      \n");
    print("- get pid             'Shells pid'                            \n");
    print("- exec [task]         'Executes a task'                       \n");
    print("- caw  [pid]          'Changes active window to provided pid' \n");
    print("- kill [pid]          'Kills task with the corresponding pid' \n");
    print("- tasks               'Lists are active tasks with their pids'\n");
    print("- exit                'Exits and kills shell'                 \n");
}

static void command_get_pid() {
    int pid = get_pid();
    char msg[10];
    itoa(pid, msg);
    print("\n");
    print("Shells pid is: ");
    print(msg);
}

static void command_exit() {
    terminate_program();
}

static void command_exec(const char *path) {
    exec(path);
}

static void command_caw(const char *target) {
    int target_pid = atoi(target);
    if (target_pid == 0 || ch_act_window(target_pid) == -1) {
        print("\nWindow change did not succeed");
    }
}

static void command_tasks() {
    char tasks[512] = {0};
    int result      = get_ac_tasks(tasks, sizeof(tasks));
    print("\n");
    if (result > 0) {
        print(tasks);
    } else {
        print("Failed to get tasks");
    }
}

static void command_kill(const char *target) {
    int target_pid = atoi(target);
    if (target_pid == 0 || kill_task(target_pid) == -1) {
        print("Failed to kill the task");
    }
}

void exec_cmd(char *buf) {
    if (str_eq(buf, "help") == 1) {
        command_help();
        return;
    }
    if (str_eq(buf, "tasks") == 1) {
        command_tasks();
        print("\n");
        return;
    }
    if (str_eq(buf, "get pid") == 1) {
        command_get_pid();
        print("\n");
        return;
    }
    if (str_eq(buf, "exit") == 1) {
        command_exit();
        print("\n");
        return;
    }
    if (str_starts_with(buf, "exec ") == 1) {
        command_exec(buf + 5);
        print("\n");
        return;
    }
    if (str_starts_with(buf, "caw ") == 1) {
        command_caw(buf + 4);
        print("\n");
        return;
    }
    if (str_starts_with(buf, "kill ") == 1) {
        command_kill(buf + 5);
        print("\n");
        return;
    }
    print("Invalid command\n");
}

void main(void) {

    if (set_operator_task() == -1) {
        return;
    }

    if (create_task_window(600, 200, 20, 780) == STATUS_ERROR) {
        return;
    }

    if (ch_bg_color(COLOR_SKY_BLUE) == STATUS_ERROR) {
        print("changing background color failed\n");
    }

    if (paint_window(590, 190, 5, 5) == STATUS_ERROR) {
        print("painting window failed\n");
    }

    if (ch_fg_color(COLOR_BLACK) == STATUS_ERROR) {
        print("changing foreground color failed\n");
    }

    /*if (paint_window(50, 50, 5 * 8, 3 * 16) == -1) {
        print("Print failed\n");
    }*/

    // vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    // vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    /* print("--------------------------------------------------------------------"
           "--------\n");
     // vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
     print("|=======|  |====|    |====|   |=|     |=|  |=======|        |====|  "
           "  |====|  \n");
     print("   |=|    |=|  |=|  |=|  |=|  |=|     |=|     |=|          |=|  |=| "
           " |=|  |=| \n");
     print("   |=|    |=----=|  |=----=|  |=|     |=|     |=|          |=|  |=| "
           "  |____   \n");
     print("   |=|    |=|  |=|  |=|  |=|   |=|   |=|      |=|          |=|  |=| "
           "       |=|\n");
     print("   |=|    |=|  |=|  |=|  |=|   |=|   |=|      |=|          |=|  |=| "
           " |=|  |=| \n");
     print("   |=|    |=|  |=|  |_|  |_|     |===|     |=======|        |====|  "
           "  |====|  \n");
     print("\n");*/
    // vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    /*print("                                               |====|     |====|
    \n"); print("                                              |=|  |=|   |=|
    |=|           \n"); print("                                              |=|
    |=|    |____             \n"); print(" |=|  |=|         |=|          \n");
    print("                                              |=|  |=|   |=|  |=|
    \n"); print("                                               |====| |====|
    \n");*/
    // vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    // print("--------------------------------------------------------------------"
    //      "-------\n");
    // vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    print("TaaviOS - Operating shell\n");
    print("Type 'help' to see all commands\n");

    char buf[256];
    int pos = 0;
    char c;

    /*fd = open("hello.txt");


     if(fd != -1) {
         print("Reading the file\n");
         int nread = read(fd, buf, 512);

         if (nread != -1) {
             print("File read succesfully\n");
             print(buf);
         } else {
             print("Read failed or file empty\n");
         }
     } else {
         print("Error on opening the file\n");
     }*/

    while (1) {
        print(">> ");
        pos = 0;
        while (1) {
            int n = scan(&c);
            if (n > 0) {

                if (pos >= 255) {
                    pos--;
                }

                if (c == '\n') {
                    buf[pos] = 0;
                    exec_cmd(buf);
                    pos = 0;
                    break;
                } else if (c == '\b') {
                    if (pos > 0) {
                        pos--;
                        print("\b");
                    }
                } else {

                    buf[pos++] = c;
                    char tmp[2];
                    tmp[0] = c;
                    tmp[1] = 0;
                    print(tmp);
                }
            }
        }
        buf[pos] = 0;
    }
}
