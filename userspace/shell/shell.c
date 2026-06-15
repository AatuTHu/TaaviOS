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

void command_write(const char *buf) {
    print("\n");
    write(fd, buf);
    print("\n");
}

void command_open(const char *filename) {
    fd = open(filename, O_RDONLY | O_WRONLY);

    if (fd != -1) {
        print("\n");
        print("Succesfully opened the file\n");
        print("\n");
    } else {
        print("\n");
        error("Error on opening the file\n");
        print("\n");
    }
}

void command_read() {
    print("\n");
    char buf[512] = {0};
    int nread     = read(fd, buf, sizeof(buf));

    if (nread != -1) {
        print(buf);
        print("\n");
    } else {
        error("Read failed or file empty\n");
        print("\n");
    }
}

void command_close() {
    print("closing file\n");
    close(fd);
    fd = -1;
}

void command_mkdir(const char *directory_name) {
    mkdir(directory_name);
}

void command_clear() {
    for (uint8_t i = 0; i < 100; i++) {
        print("\n");
    }
}

void command_cd(const char *path) {
    change_directory(path, dir_name);
}

void command_ls() {
    char buf[512] = {0};
    list_dirents(buf, sizeof(buf));

    print(buf);
}

void exec_cmd(char *buf) {
    print("\n");
    if (str_eq(buf, "clear") == 1)
        command_clear();
    if (str_eq(buf, "help") == 1)
        command_help();
    if (str_eq(buf, "about") == 1)
        command_about();
    if (str_eq(buf, "get pid") == 1)
        command_get_pid();
    if (str_eq(buf, "exit") == 1)
        command_exit();
    if (str_eq(buf, "close") == 1)
        command_close();
    if (str_eq(buf, "read") == 1)
        command_read();
    if (str_eq(buf, "ls") == 1)
        command_ls();
    if (str_starts_with(buf, "write ") == 1)
        command_write(buf + 6);
    if (str_starts_with(buf, "open ") == 1)
        command_open(buf + 5);
    if (str_starts_with(buf, "exec ") == 1)
        command_exec(buf + 5);
    if (str_starts_with(buf, "mkdir ") == 1)
        command_mkdir(buf + 6);
    if (str_starts_with(buf, "cd ") == 1)
        command_cd(buf + 3);

    print("\n");
}

void main(void) {
    // vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    // vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    print("--------------------------------------------------------------------"
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
    print("\n");
    // vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    /*print("                                               |====|     |====|
    \n"); print("                                              |=|  |=|   |=|
    |=|           \n"); print("                                              |=|
    |=|    |____             \n"); print(" |=|  |=|         |=|          \n");
    print("                                              |=|  |=|   |=|  |=|
    \n"); print("                                               |====| |====|
    \n");*/
    // vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    print("--------------------------------------------------------------------"
          "-------\n");
    // vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    print("TaaviOS Shell\n");
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
        print(" >> ");
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