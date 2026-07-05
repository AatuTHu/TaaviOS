#include "stand.h"
#include "string.h"
#include <stddef.h>
#include <stdint.h>

static int fd = -1;
static char dir_name[8];

void command_help() {
    print("\n------------------------------------------------------------------"
          "-------------\n");
    print("Available commands\n");
    print("- help               'Prints this list'          \n");
    print("- about              'Useless function'          \n");
    print("- get my pid         'Shells pid'                \n");
    print("- exec  [task]       'Executes a task'           \n");
    print("- open  [path]       'Open a file'               \n");
    print("- write [text]       'Writes to opened file'     \n");
    print("- mkdir [text]       'Creates a directory'       \n");
    print("- cd    [text]       'Changes working directory' \n");
    print("- read               'Reads the opened file'     \n");
    print("- close              'close opened file'         \n");
    print("- exit               'Exits and kills shell'     \n");
    print("\n");
}

void command_about() {
    print("\n---------------------------------------------------------------\n");
    print("TaaviOS v0.6.0 - Author: Aatu H\n");
}

void command_get_pid() {
    int pid = get_pid();
    char msg[10];
    itoa(pid, msg);
    print("\n");
    print("current task pid: ");
    print(msg);
    print("\n");
}

void command_exit() {
    terminate_program();
}

void command_exec(const char *path) {
    exec(path);
}

void command_clear() {
    for (uint8_t i = 0; i < 100; i++) {
        print("\n");
    }
}

void exec_cmd(char *buf) {
    print("\n");
    if (str_eq(buf, "clear") == 1) {
        command_clear();
        print("\n");
        return;
    }
    if (str_eq(buf, "help") == 1) {
        command_help();
        print("\n");
        return;
    }
    if (str_eq(buf, "about") == 1) {
        command_about();
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
    print("Invalid command\n");
}

void main(void) {
    if (create_task_window(600, 200, 20, 780) == -1) {
        return;
    }
    if (configurate_task_window(0, 0, 0, 0) == -1) {
        return;
    }
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
        print("\n");
        print(dir_name);
        print(">> ");
        pos = 0;
        while (1) {
            int n = scan(&c);
            if (n > 0) {
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