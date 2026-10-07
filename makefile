ARCH ?= i686
LOG_LEVEL ?= 2

KERNEL_DIR := kernel
KERNEL_SHARED_DIR := kernel/shared

ifeq ($(ARCH),x86_64)
    AS          := nasm
    CC          := x86_64-elf-gcc
    LD          := x86_64-elf-ld
    QEMU        := qemu-system-x86_64
    ASFLAGS     := -f elf64
    ARCH_FLAGS  := -mcmodel=kernel -mno-red-zone
    KERNEL_ARCH_DIR := kernel/arch/x86_64
    ARCH_INC    := $(KERNEL_ARCH_DIR)/include/x86_64
    LINKER_SCRIPT := boot/linker64.ld
    LDFLAGS     := -m elf_x86_64 -T $(LINKER_SCRIPT) -z noexecstack
else ifeq ($(ARCH),i686)
    AS          := nasm
    CC          := i686-elf-gcc
    LD          := i686-elf-ld
    QEMU        := qemu-system-i386
    ASFLAGS     := -f elf32
    ARCH_FLAGS  := -m32
    KERNEL_ARCH_DIR := kernel/arch/x86
    ARCH_INC    := $(KERNEL_ARCH_DIR)/include/i386
    LINKER_SCRIPT := boot/linker.ld
    LDFLAGS     := -melf_i386 -T $(LINKER_SCRIPT) -z noexecstack
else
    $(error Unsupported architecture $(ARCH). Use ARCH=x86_64 or ARCH=i686)
endif

INCLUDES := -I$(KERNEL_ARCH_DIR)/include \
            -I$(ARCH_INC) \
            -I$(KERNEL_ARCH_DIR)/include/drivers \
            -I$(KERNEL_ARCH_DIR)/include/mm \
            -I$(KERNEL_ARCH_DIR)/include/loader \
            -I$(KERNEL_ARCH_DIR)/include/kernel_clerks \
            -I$(KERNEL_ARCH_DIR)/include/protocols \
            -I$(KERNEL_DIR)/include \
            -I$(KERNEL_DIR)/include/shared \
            -I$(KERNEL_DIR)/include/drivers \
            -I$(KERNEL_DIR)/include/libraries \
            -I$(KERNEL_DIR)/include/fs

CFLAGS := -g -ffreestanding -O2 -nostdlib -Wall -Wextra \
          $(ARCH_FLAGS) -mno-sse -mno-sse2 -mno-mmx \
          $(INCLUDES) \
          -fno-pic -fno-stack-protector \
          -fno-asynchronous-unwind-tables -fno-exceptions \
          -DLOG_LEVEL=$(LOG_LEVEL)

BUILD := build/$(ARCH)

C_SRCS   := $(shell find $(KERNEL_DIR) -name '*.c' ! -path 'kernel/arch/*' 2>/dev/null) \
            $(shell find $(KERNEL_ARCH_DIR) -name '*.c' 2>/dev/null)

ASM_SRCS := $(shell find boot $(KERNEL_ARCH_DIR) -name '*.asm' 2>/dev/null) \
            $(shell find $(KERNEL_DIR) -name '*.asm' ! -path 'kernel/arch/*' 2>/dev/null)

C_OBJS   := $(patsubst %.c,   $(BUILD)/%.o, $(C_SRCS))
ASM_OBJS := $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))
OBJS     := $(ASM_OBJS) $(C_OBJS)

.PHONY: all iso debug run clean disk reset check format gdb

all: $(BUILD)/taavi.bin

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(dir $@)
	@$(AS) $(ASFLAGS) $< -o $@

$(BUILD)/taavi.bin: $(OBJS)
	@mkdir -p $(BUILD)
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(BUILD)/taavi.bin
	@$(MAKE) --no-print-directory -C userspace ARCH=$(ARCH) > /dev/null 2>&1 || true
	@mkdir -p isodir/boot/grub
	@cp $(BUILD)/taavi.bin isodir/boot/
	@cp userspace/build/bin/*.elf isodir/boot/ 2>/dev/null || true
	@cp grub.cfg isodir/boot/grub/grub.cfg
	@grub-mkrescue -o $(BUILD)/taavi.iso isodir > /dev/null 2>&1

debug: iso
	@mkdir -p $(BUILD)
	@$(QEMU) \
		-drive file=$(BUILD)/taavi.iso,format=raw,if=ide,bus=0,unit=0,media=cdrom \
		-drive file=fat.img,format=raw,if=ide,bus=0,unit=1,media=disk \
		-boot d -serial stdio -no-reboot -no-shutdown -s -S

run: iso
	@mkdir -p $(BUILD)
	@$(QEMU) \
		-drive file=$(BUILD)/taavi.iso,format=raw,if=ide,bus=0,unit=0,media=cdrom \
		-drive file=fat.img,format=raw,if=ide,bus=0,unit=1,media=disk \
		-boot d -serial stdio -no-reboot -no-shutdown -d int,cpu_reset 2>$(BUILD)/qemu_log.txt

clean:
	@$(MAKE) --no-print-directory -C userspace clean > /dev/null 2>&1 || true
	@rm -rf build isodir
	@rm -f cppcheck_report.txt

disk:
	@dd if=/dev/zero of=fat.img bs=1M count=64 status=none
	@printf 'label: dos\ntype=0C, bootable\n' | sfdisk fat.img > /dev/null 2>&1
	@mformat -i fat.img@@1M -F -v TAAVI_FAT
	@mmd -i fat.img@@1M ::/sysbin ::/test
	@echo "Hello from TaaviOS!" | mcopy -i fat.img@@1M - ::/test/hello.txt
	@for f in userspace/build/bin/*.elf; do \
		if [ -f "$$f" ]; then \
			name=$$(basename $$f .elf); \
			if [ "$$name" != "init" ]; then \
				mcopy -i fat.img@@1M $$f ::/sysbin/$$name; \
			fi; \
		fi; \
	done

reset:
	@echo "Rebuilding OS..."
	@$(MAKE) --no-print-directory iso ARCH=$(ARCH)
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
