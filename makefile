ARCH ?= i686
LOG_LEVEL ?= 2

KERNEL_DIR := kernel
KERNEL_SHARED_DIR := kernel/shared
LIMINE_DIR ?= $(HOME)/projects/TaaviOS_TOOLS/limine
LIMINE_CONF := limine.conf
OVMF        ?=

BUILD := build/$(ARCH)
USER_BIN := userspace/build/$(ARCH)/bin

-include tools/$(ARCH).mk

INCLUDES := -I$(KERNEL_ARCH_DIR)/include \
            -I$(ARCH_INC) \
            -I$(KERNEL_DIR)/include/drivers \
            -I$(KERNEL_DIR)/include/loader \
            -I$(KERNEL_DIR)/include/kernel_clerks \
            -I$(KERNEL_DIR)/include/protocols \
            -I$(KERNEL_DIR)/include/boot \
            -I$(KERNEL_DIR)/include/mm \
            -I$(KERNEL_DIR)/include \
            -I$(KERNEL_DIR)/include/shared \
            -I$(KERNEL_DIR)/include/libraries \
            -I$(KERNEL_DIR)/include/fs

CFLAGS := -g -ffreestanding -O2 -nostdlib -Wall -Wextra \
          $(ARCH_FLAGS) $(SIMD_FLAGS) \
          $(INCLUDES) \
          -fno-pic -fno-pie -fno-stack-protector \
          -fno-asynchronous-unwind-tables -fno-exceptions \
          -DLOG_LEVEL=$(LOG_LEVEL)

C_SRCS   := $(shell find $(KERNEL_DIR) -name '*.c' ! -path 'kernel/arch/*' 2>/dev/null) \
            $(shell find $(KERNEL_ARCH_DIR) -name '*.c' 2>/dev/null)

ASM_SRCS := $(shell find $(BOOT_ASM_DIR) $(KERNEL_ARCH_DIR) -name '*.asm' 2>/dev/null) \
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

$(BUILD)/taavi.bin: $(OBJS) $(LINKER_SCRIPT)
	@mkdir -p $(BUILD)
	@$(LD) $(LDFLAGS) -o $@ $(OBJS)

debug: iso
	@mkdir -p $(BUILD)
	@$(QEMU) $(QEMU_FW) \
		-smp 4 \
		-drive file=$(BUILD)/taavi.iso,format=raw,if=ide,bus=0,unit=0,media=cdrom \
		-drive file=fat.img,format=raw,if=ide,bus=0,unit=1,media=disk \
		-boot d -serial stdio -no-reboot -no-shutdown -s -S

run: iso
	@mkdir -p $(BUILD)
	@$(QEMU) $(QEMU_FW) \
		-smp 4 \
		-drive file=$(BUILD)/taavi.iso,format=raw,if=ide,bus=0,unit=0,media=disk \
		-drive file=fat.img,format=raw,if=ide,bus=0,unit=1,media=disk \
		-boot d -serial stdio -no-reboot -no-shutdown -d int,cpu_reset 2>$(BUILD)/qemu_log.txt

clean:
	@$(MAKE) --no-print-directory -C userspace clean > /dev/null 2>&1 || true
	@rm -rf build isodir iso_root
	@rm -f cppcheck_report.txt

disk:
	@dd if=/dev/zero of=fat.img bs=1M count=64 status=none
	@printf 'label: dos\ntype=0C, bootable\n' | sfdisk fat.img > /dev/null 2>&1
	@mformat -i fat.img@@1M -F -v TAAVI_FAT
	@mmd -i fat.img@@1M ::/sysbin ::/test
	@echo "Hello from TaaviOS!" | mcopy -i fat.img@@1M - ::/test/hello.txt
	@for f in $(USER_BIN)/*.elf; do \
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
	@$(MAKE) --no-print-directory disk ARCH=$(ARCH)
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
