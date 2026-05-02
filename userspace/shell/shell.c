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
    write("- cpu uptime\n");
    write("- exit\n");
}

void command_about() {
    write("\n-------------------------------------------------------------------------------\n");
    write("Carrots v0.1.0 - Author: Aatu H\n");
}

void command_cpu_uptime() {
    /*int uptime = sys_uptime();
    char msg[20];
    itoa(uptime, msg);
    write(": ");
    write(msg);
    write("ms\n");*/
}

void command_exit() {
    terminate_program();
}

void command_exec(const char *filename) {
    //sys_exec(filename);
}

void exec_cmd(char *buf) {
    if(str_eq(buf, "help") == 1) command_help();
    if(str_eq(buf, "about") == 1) command_about();
    if(str_eq(buf, "cpu uptime") == 1) command_cpu_uptime();
    if(str_eq(buf, "exit") == 1) command_exit();
    if(str_starts_with(buf, "exec ")==1) {
        command_exec(buf + 5);
        write("\n");
        return;
    }
}

void main(void) {
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
            if (n <= 0) continue;

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