#include <stdint.h>
#include <stddef.h>
#include "stand.h"
#include "string.h"

static int fd = -1;


void command_help() {
    print("\n-------------------------------------------------------------------------------\n");
    print("Available commands\n");
    print("- help\n");
    print("- about\n");
    print("- get my pid\n");
    print("- exec  [task]\n");
    print("- open  [path]\n");
    print("- write [text] \n");
    print("- exit\n");
    print("\n");
}

void command_about() {
    print("\n-------------------------------------------------------------------------------\n");
    print("Carrots v0.5.0 - Author: Aatu H\n");
    print("\n");
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
    print("\n");
    terminate_program();
    print("\n");
}

void command_exec(const char *filename) {
    print("\n");
    exec(filename);
    print("\n");
}

void command_write(const char *buf) {
    write(4,"wake!");
}

void command_open(const char *filename) {
    print("\n");
    fd = open(filename);

    
    if(fd != -1) {
        print("Succesfully opened the file\n");
    } else {
        print("Error on opening the file\n");
    }
}

void command_read() {
    print("\n");
    if(fd == -1) {
        print("Cant read the file\n");
    } else {
        print("Reading the file\n");
        char buf[512] = {0}; 

        int nread = read(fd, buf, 512); 
        
        if (nread != -1) {
            print("File read succesfully\n");
            print(buf);
        } else {
            print("Read failed or file empty\n");
        }
    }
}

void exec_cmd(char *buf) {
    if(str_eq(buf, "help") == 1) command_help();
    if(str_eq(buf, "about") == 1) command_about();
    if(str_eq(buf, "get pid") == 1) command_get_pid();
    if(str_eq(buf, "exit") == 1) command_exit();
    if(str_eq(buf, "read")==1) command_read();

    if(str_starts_with(buf, "write ")==1) {
            command_write(buf + 6);
            return;
    }
    if(str_starts_with(buf, "open ")==1) {
        command_open(buf + 5);
        return;
    }
    if(str_starts_with(buf, "exec ")==1) {
        command_exec(buf + 5);
        return;
    }

}

void main(void) {
    //vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    print("----------------------------------------------------------------------------\n");
    //vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    print("  |=====|    |====|    |====|                                               \n");
    print(" |=|   |=|  |=|  |=|  |=|  |=|                                              \n");
    print(" |=|       |=|    |=| |=----=|                                              \n");
    print(" |=|       |=------=| |=|===|                                               \n");
    print(" |=|   |=| |=|    |=| |=|  |==|                                             \n");
    print("  |=====|  |=|    |=| |=|   |==|                                            \n");
    //vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    print("                               |====|     |====| |=======|  |=====|        \n");
    print("                              |=|  |=|   |=|  |=|   |=|    |=|   |=|       \n");
    print("                              |=----=|   |=|  |=|   |=|    |=|___          \n");
    print("                              |=|===|    |=|  |=|   |=|          |=|       \n");
    print("                              |=|  |==|  |=|  |=|   |=|    |=|   |=|       \n");
    print("                              |=|   |==|  |====|    |=|     |=====|        \n");
    //vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    print("---------------------------------------------------------------------------\n");
    //vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    print("Carrots Shell\n");
    print("Type 'help' to see all commands\n");
    
    char buf[256];
    int pos = 0;
    char c;
    
    while (1) {
        error("\n> ");
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
                   if(pos > 0) {
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