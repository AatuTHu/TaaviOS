AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld
ASFLAGS = -f elf32
LOG_LEVEL ?= 2
CFLAGS = -ffreestanding -O2 -nostdlib -Wall -Wextra \
         -Ikernel/include -Ikernel/include/i386 -Ikernel/include/drivers -Ikernel/include/libraries \
		 -Ikernel/include/mm -Ikernel/include/tcb -Ikernel/include/loader -Ikernel \
		 -Ikernel/include/usermode -Ikernel/include/fs -Ikernel/include/kernel_clerks \
		 -Ikernel/include/protocols \
         -fno-pic -fno-stack-protector \
         -fno-asynchronous-unwind-tables -fno-exceptions \
		 -mno-sse -mno-sse2 -mno-mmx \
         -DLOG_LEVEL=$(LOG_LEVEL)
LDFLAGS = -melf_i386 -T boot/linker.ld -z noexecstack
BUILD = build
C_SRCS   = $(shell find kernel -name '*.c')
ASM_SRCS = $(shell find kernel boot -name '*.asm')
C_OBJS   = $(patsubst %.c,   $(BUILD)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))
OBJS     = $(ASM_OBJS) $(C_OBJS)
all: $(BUILD)/taavi.bin
$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@
$(BUILD)/%.o: %.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@
$(BUILD)/taavi.bin: $(OBJS)
	$(LD) $(LDFLAGS) -o $@ $(OBJS)
iso: $(BUILD)/taavi.bin
	$(MAKE) -C userspace
	mkdir -p isodir/boot/grub
	cp $(BUILD)/taavi.bin isodir/boot/
	cp userspace/build/bin/*.elf isodir/boot/
	cp grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o $(BUILD)/taavi.iso isodir

run: iso
	qemu-system-i386 \
		-drive file=$(BUILD)/taavi.iso,format=raw,if=ide,bus=0,unit=0,media=cdrom \
		-drive file=fat.img,format=raw,if=ide,bus=0,unit=1,media=disk \
		-boot d -serial stdio -no-reboot -no-shutdown -d int,cpu_reset 2>$(BUILD)/qemu_log.txt

clean:
	$(MAKE) -C userspace clean
	find . -name '*.o' -delete
	rm -f taavi.bin taavi.iso
	rm -rf $(BUILD) isodir
	rm -f cppcheck_report.txt

disk:
	dd if=/dev/zero of=fat.img bs=1M count=64
	mkfs.fat -F 32 fat.img
	mmd -i fat.img ::sysbin
	mmd -i fat.img ::test
	mmd -i fat.img ::test/tust
	echo "Hello from Taavi OS!" | mcopy -i fat.img - ::test/hello.txt
	mcopy -i fat.img ./isodir/boot/shell.elf ::sysbin/shell

check:
	cppcheck \
		--enable=all --inconclusive --language=c \
		--check-level=exhaustive \
		--language=c \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction \
		-I kernel/include \
		-I kernel/include/i386 \
		-I kernel/include/drivers \
		-I kernel/include/libraries \
		-I kernel/include/mm \
		-I kernel/include/tcb \
		-I kernel/include/loader \
		-I kernel/include/usermode \
		-I kernel/include/fs \
		-I kernel/include/kernel_clerks \
		-I kernel/include/protocols \
		kernel/ \
		2>&1 | tee cppcheck_report.txt

format:
	@echo "Formatting kernel source files..."
	@find $(KERNEL_DIR) -name "*.c" -o -name "*.h" | xargs clang-format -i
	@echo "Kernel formatting complete."