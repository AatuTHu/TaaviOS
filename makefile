AS = nasm
CC = gcc
LD = ld
ASFLAGS = -f elf32

CFLAGS = -m32 -ffreestanding -O2 -nostdlib -Wall -Wextra \
         -Ikernel/include -Ikernel \
         -fno-pic -fno-stack-protector \
         -fno-asynchronous-unwind-tables -fno-exceptions

LDFLAGS = -melf_i386 -T boot/linker.ld -z noexecstack

C_SRCS = $(shell find kernel -name '*.c')
ASM_SRCS = $(shell find kernel boot -name '*.asm')
C_OBJS = $(C_SRCS:.c=.o)
ASM_OBJS = $(ASM_SRCS:.asm=.o)
OBJS = $(ASM_OBJS) $(C_OBJS)

all: carrots.bin

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

carrots.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o carrots.bin $(OBJS)

iso: carrots.bin
	mkdir -p isodir/boot/grub
	cp carrots.bin isodir/boot/
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o carrots.iso isodir

run: iso
	qemu-system-i386 -cdrom carrots.iso -serial stdio -no-reboot -no-shutdown -d int,cpu_reset 2>qemu_log.txt

clean:
	find . -name '*.o' -delete
	rm -f carrots.bin carrots.iso
	rm -rf isodir

.PHONY: all iso run clean