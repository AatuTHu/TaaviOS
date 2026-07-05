#include "stand.h"
#include "string.h"
#include <stddef.h>
#include <stdint.h>

static int fd = -1;
static char dir_name[8];

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
    if (str_starts_with(buf, "mkdir ") == 1)
        command_mkdir(buf + 6);
    if (str_starts_with(buf, "cd ") == 1)
        command_cd(buf + 3);
    print("\n");
}

void main(void) {
    if (create_task_window(600, 200, 20, 20) == -1) {
        return;
    }
    if (configurate_task_window(0, 0, 0, 0) == -1) {
        return;
    }

    print("fs_interface\n");
    char buf[256];
    int pos = 0;
    char c;

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