#include <stdint.h>
#include <stddef.h>
#include "stand.h"
#include "string.h"

void command_help() {
    write("\n-------------------------------------------------------------------------------\n");
    write("Available commands\n");
    write("- help\n");
    write("- about\n");
    write("- exec filename.elf \n");
    write("- get pid\n");
    write("- exit\n");
    write("\n");
}

void command_about() {
    write("\n-------------------------------------------------------------------------------\n");
    write("Carrots v0.3.0 - Author: Aatu H\n");
    write("\n");
}

void command_get_pid() {
    int pid = get_pid();
    char msg[10];
    itoa(pid, msg);
    write("\n");
    write("current task pid: ");
    write(msg);
    write("\n");
}

void command_exit() {
    write("\n");
    terminate_program();
    write("\n");
}

void command_exec(const char *filename) {
    write("\n");
    exec(filename);
    write("\n");
}

void command_wake() {
    fwrite(4,"wake!");
}

void exec_cmd(char *buf) {
    if(str_eq(buf, "help") == 1) command_help();
    if(str_eq(buf, "about") == 1) command_about();
    if(str_eq(buf, "get pid") == 1) command_get_pid();
    if(str_eq(buf, "exit") == 1) command_exit();
    if(str_eq(buf, "wake") == 1) command_wake();
    if(str_starts_with(buf, "exec ")==1) {
        command_exec(buf + 5);
        return;
    }

}

void main(void) {
    //vga_set_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK);
    write("----------------------------------------------------------------------------\n");
    //vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    write("  |=====|    |====|    |====|                                               \n");
    write(" |=|   |=|  |=|  |=|  |=|  |=|                                              \n");
    write(" |=|       |=|    |=| |=----=|                                              \n");
    write(" |=|       |=------=| |=|===|                                               \n");
    write(" |=|   |=| |=|    |=| |=|  |==|                                             \n");
    write("  |=====|  |=|    |=| |=|   |==|                                            \n");
    //vga_set_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK);
    write("                               |====|     |====| |=======|  |=====|        \n");
    write("                              |=|  |=|   |=|  |=|   |=|    |=|   |=|       \n");
    write("                              |=----=|   |=|  |=|   |=|    |=|___          \n");
    write("                              |=|===|    |=|  |=|   |=|          |=|       \n");
    write("                              |=|  |==|  |=|  |=|   |=|    |=|   |=|       \n");
    write("                              |=|   |==|  |====|    |=|     |=====|        \n");
    //vga_set_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK);
    write("---------------------------------------------------------------------------\n");
    //vga_set_color(VGA_COLOR_DARK_GREY, VGA_COLOR_BLACK);
    write("Carrots Shell\n");
    write("Type 'help' to see all commands\n");
    
    char buf[256];
    int pos = 0;
    char c;
    
    while (1) {
        write("\n> ");
        pos = 0;
        while (1) {
            int n = read(&c);
            if (n > 0) {
                if (c == '\n') {
                    buf[pos] = 0;
                    exec_cmd(buf);
                    pos = 0;
                    break;
                } else if (c == '\b') {
                   if(pos > 0) {
                        pos--;
                        write("\b");
                   }
                } else {
                    buf[pos++] = c;
                    char tmp[2];
                    tmp[0] = c;
                    tmp[1] = 0;
                    write(tmp);
                }
            }
        }
        buf[pos] = 0;
    }
}