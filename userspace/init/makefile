CC = gcc
CFLAGS = -m32 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -O0 -fno-pic -fno-pie -mno-accumulate-outgoing-args -mregparm=0 -fno-stack-protector
LDFLAGS = -m32 -nostdlib -no-pie -T link.ld

LIBC_OBJS = $(LIBC_SRCS:.c=.o)

all: init.elf

START_OBJ = start.o

start.o: start.asm
	nasm -f elf32 start.asm -o start.o

init.elf: start.o main.o $(LIBC_OBJS)
	$(CC) $(LDFLAGS) -o init.elf start.o main.o $(LIBC_OBJS)

main.o: main.c
	$(CC) $(CFLAGS) -c main.c -o main.o

clean:
	rm -f *.o init.elf