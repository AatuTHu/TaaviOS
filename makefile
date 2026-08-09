AS = nasm
CC = i686-elf-gcc
LD = i686-elf-ld
ASFLAGS = -f elf32
LOG_LEVEL ?= 2

KERNEL_DIR = kernel

INCLUDES = -I$(KERNEL_DIR)/include -I$(KERNEL_DIR)/include/i386 -I$(KERNEL_DIR)/include/drivers \
           -I$(KERNEL_DIR)/include/libraries -I$(KERNEL_DIR)/include/mm -I$(KERNEL_DIR)/include/tcb \
           -I$(KERNEL_DIR)/include/loader -I$(KERNEL_DIR)/include/usermode -I$(KERNEL_DIR)/include/fs \
           -I$(KERNEL_DIR)/include/kernel_clerks -I$(KERNEL_DIR)/include/protocols \
           -I$(KERNEL_DIR)/include/shared -I$(KERNEL_DIR)

CFLAGS = -g -ffreestanding -O2 -nostdlib -Wall -Wextra \
         $(INCLUDES) \
         -fno-pic -fno-stack-protector \
         -fno-asynchronous-unwind-tables -fno-exceptions \
         -mno-sse -mno-sse2 -mno-mmx \
         -DLOG_LEVEL=$(LOG_LEVEL)

LDFLAGS = -melf_i386 -T boot/linker.ld -z noexecstack

BUILD = build

C_SRCS   = $(shell find $(KERNEL_DIR) -name '*.c')
ASM_SRCS = $(shell find $(KERNEL_DIR) boot -name '*.asm')
C_OBJS   = $(patsubst %.c,   $(BUILD)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))
OBJS     = $(ASM_OBJS) $(C_OBJS)

.PHONY: all iso debug run clean disk reset check format gdb

all: $(BUILD)/taavi.bin

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/taavi.bin: $(OBJS)
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(BUILD)/taavi.bin
	@$(MAKE) --no-print-directory -C userspace > /dev/null
	@mkdir -p isodir/boot/grub
	@cp $(BUILD)/taavi.bin isodir/boot/
	@cp userspace/build/bin/*.elf isodir/boot/ 2>/dev/null || true
	@cp grub.cfg isodir/boot/grub/grub.cfg
	@grub-mkrescue -o $(BUILD)/taavi.iso isodir > /dev/null 2>&1

debug: iso
	@qemu-system-i386 \
		-drive file=$(BUILD)/taavi.iso,format=raw,if=ide,bus=0,unit=0,media=cdrom \
		-drive file=fat.img,format=raw,if=ide,bus=0,unit=1,media=disk \
		-boot d -serial stdio -no-reboot -no-shutdown -s -S

run: iso
	@qemu-system-i386 \
		-drive file=$(BUILD)/taavi.iso,format=raw,if=ide,bus=0,unit=0,media=cdrom \
		-drive file=fat.img,format=raw,if=ide,bus=0,unit=1,media=disk \
		-boot d -serial stdio -no-reboot -no-shutdown -d int,cpu_reset 2>$(BUILD)/qemu_log.txt

clean:
	@$(MAKE) --no-print-directory -C userspace clean > /dev/null 2>&1 || true
	@rm -rf $(BUILD) isodir
	@rm -f cppcheck_report.txt

disk:
	@dd if=/dev/zero of=fat.img bs=1M count=64 status=none
	@parted -s fat.img mklabel msdos
	@parted -s fat.img mkpart primary fat32 1MiB 100%
	@sudo losetup -Pf --show fat.img > /tmp/taavi_loop.txt
	@sudo mkfs.fat -F 32 $$(cat /tmp/taavi_loop.txt)p1 > /dev/null
	@sudo mkdir -p /mnt/taavi
	@sudo mount $$(cat /tmp/taavi_loop.txt)p1 /mnt/taavi
	@sudo mkdir -p /mnt/taavi/sysbin /mnt/taavi/test
	@echo "Hello from TaaviOS!" | sudo tee /mnt/taavi/test/hello.txt > /dev/null
	@for f in userspace/build/bin/*.elf; do \
		if [ -f "$$f" ]; then \
			name=$$(basename $$f .elf); \
			if [ "$$name" != "init" ]; then \
				sudo cp $$f /mnt/taavi/sysbin/$$name; \
			fi; \
		fi; \
	done
	@sudo umount /mnt/taavi
	@sudo losetup -d $$(cat /tmp/taavi_loop.txt)
	@rm /tmp/taavi_loop.txt

reset:
	@echo "Rebuilding OS..."
	@$(MAKE) --no-print-directory iso
	@echo "Rebuilding disk image..."
	@$(MAKE) --no-print-directory disk
	@echo "Reset complete."

check:
	@cppcheck \
		--enable=all --inconclusive --language=c \
		--check-level=exhaustive \
		--suppress=missingIncludeSystem \
		--suppress=unusedFunction \
		$(INCLUDES) \
		$(KERNEL_DIR)/ \
		2>&1 | tee cppcheck_report.txt

format:
	@echo "Formatting kernel source files..."
	@find $(KERNEL_DIR) -name "*.c" -o -name "*.h" | xargs clang-format -i
	@echo "Kernel formatting complete."

gdb: $(BUILD)/taavi.bin
	@gdb -ex "file $(BUILD)/taavi.bin" -ex "target remote localhost:1234"
