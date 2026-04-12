AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld
ASFLAGS = -f elf32
CFLAGS = -ffreestanding -O2 -nostdlib -Wall -Wextra \
         -Ikernel/include -Ikernel/include/i386 -Ikernel/include/drivers -Ikernel/include/libraries \
		-Ikernel/include/mm -Ikernel/include/proc -Ikernel/include/loader -Ikernel -Ikernel/include/syscall \
         -fno-pic -fno-stack-protector \
         -fno-asynchronous-unwind-tables -fno-exceptions \
		 -mno-sse -mno-sse2 -mno-mmx
LDFLAGS = -melf_i386 -T boot/linker.ld -z noexecstack

BUILD = build

C_SRCS   = $(shell find kernel -name '*.c')
ASM_SRCS = $(shell find kernel boot -name '*.asm')

C_OBJS   = $(patsubst %.c,   $(BUILD)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))
OBJS     = $(ASM_OBJS) $(C_OBJS)

all: $(BUILD)/carrots.bin

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/carrots.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(BUILD)/carrots.bin
	mkdir -p isodir/boot/grub
	cp $(BUILD)/carrots.bin isodir/boot/
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/carrots.iso isodir

run: iso
	qemu-system-i386 -cdrom $(BUILD)/carrots.iso -serial stdio -no-reboot -no-shutdown -d int,cpu_reset 2>$(BUILD)/qemu_log.txt

clean:
	find . -name '*.o' -delete
	rm -f carrots.bin carrots.iso
	rm -rf $(BUILD) isodir

.PHONY: all iso run clean