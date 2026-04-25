#include <stdint.h>
#include <stddef.h>

void sys_write(const char *msg) {
    int len = 1;
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(4), "b"(1), "c"(msg), "d"(len)
        : "memory"
    );
}

void sys_exit(void) {
    __asm__ __volatile__(
        "int $0x80"
        :
        : "a"(1)
    );
}

int sys_read(char *buf, int len) {
    int result;
    __asm__ __volatile__(
        "int $0x80"
        : "=a"(result)
        : "a"(3), "b"(0), "c"(buf), "d"(len)
        : "memory"
    );

    return result;
}

void main(void) {
    sys_write("Carrots Shell\n");
    sys_write("Type 'help' to see all commands\n");
    
    char buf[256];
    int pos = 0;
    char c;
    
    while (1) {
        sys_write("\n> ");
        pos = 0;
        while (1) {
            int n = sys_read(&c, 1);
            if (n > 0) {
                if (c == '\n') {
                    buf[pos] = 0;
                    pos = 0;
                    break;
                } else if (c == '\b') {
                   if(pos > 0) {
                        pos--;
                        sys_write("\b");
                   }
                } else {
                    buf[pos++] = c;
                    char tmp[2];
                    tmp[0] = c;
                    tmp[1] = 0;
                    sys_write(tmp);
                }
            }
        }
        buf[pos] = 0;
    }
}