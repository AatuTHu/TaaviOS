#include <stdint.h>
#include <stddef.h>

void sys_write(const char *msg, int len) {
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
   sys_write("> \n",4);
   sys_write("> \n",4);
   sys_write("> \n",4);
   sys_write("> \n",4);
   sys_write("> \n",4);
   sys_write("> \n",4);
   sys_write("> \n",4);
   sys_write("> \n",4);

   sys_write("> \n",4);
   sys_write("> \n",4);
   sys_write("> \n",4);
}