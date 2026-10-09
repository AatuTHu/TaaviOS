ARCH ?= i686
LOG_LEVEL ?= 2

KERNEL_DIR := kernel
KERNEL_SHARED_DIR := kernel/shared

LIMINE_DIR ?= $(HOME)/projects/TaaviOS_TOOLS/limine
LIMINE_CONF := limine.conf
OVMF        ?=

ifeq ($(ARCH),x86_64)
    AS          := nasm
    CC          := x86_64-elf-gcc
    LD          := x86_64-elf-ld
    QEMU        := qemu-system-x86_64
    ASFLAGS     := -f elf64
    ARCH_FLAGS  := -m64 -mcmodel=kernel -mno-red-zone -mgeneral-regs-only
    SIMD_FLAGS  :=
    KERNEL_ARCH_DIR := kernel/arch/x86_64
    ARCH_INC    := $(KERNEL_ARCH_DIR)/include/x86_64
    BOOT_ASM_DIR :=
    LINKER_SCRIPT := boot/linker64.ld
    LDFLAGS     := -m elf_x86_64 -T $(LINKER_SCRIPT) -z noexecstack \
                   -z max-page-size=0x1000 -nostdlib
    QEMU_FW     := $(if $(OVMF),-bios $(OVMF))
else ifeq ($(ARCH),i686)
    AS          := nasm
    CC          := i686-elf-gcc
    LD          := i686-elf-ld
    QEMU        := qemu-system-i386
    ASFLAGS     := -f elf32
    ARCH_FLAGS  := -m32
    SIMD_FLAGS  := -mno-sse -mno-sse2 -mno-mmx
    KERNEL_ARCH_DIR := kernel/arch/x86
    ARCH_INC    := $(KERNEL_ARCH_DIR)/include/i386
    BOOT_ASM_DIR := boot/i686
    LINKER_SCRIPT := boot/linker.ld
    LDFLAGS     := -melf_i386 -T $(LINKER_SCRIPT) -z noexecstack
    QEMU_FW     :=
else
    $(error Unsupported architecture $(ARCH). Use ARCH=x86_64 or ARCH=i686)
endif

INCLUDES := -I$(KERNEL_ARCH_DIR)/include \
            -I$(ARCH_INC) \
            -I$(KERNEL_ARCH_DIR)/include/drivers \
            -I$(KERNEL_ARCH_DIR)/include/loader \
            -I$(KERNEL_ARCH_DIR)/include/kernel_clerks \
            -I$(KERNEL_ARCH_DIR)/include/protocols \
			-I$(KERNEL_DIR)/include/boot \
            -I$(KERNEL_DIR)/include/mm \
            -I$(KERNEL_DIR)/include \
            -I$(KERNEL_DIR)/include/shared \
            -I$(KERNEL_DIR)/include/drivers \
            -I$(KERNEL_DIR)/include/libraries \
            -I$(KERNEL_DIR)/include/fs

CFLAGS := -g -ffreestanding -O2 -nostdlib -Wall -Wextra \
          $(ARCH_FLAGS) $(SIMD_FLAGS) \
          $(INCLUDES) \
          -fno-pic -fno-pie -fno-stack-protector \
          -fno-asynchronous-unwind-tables -fno-exceptions \
          -DLOG_LEVEL=$(LOG_LEVEL)

BUILD := build/$(ARCH)

C_SRCS   := $(shell find $(KERNEL_DIR) -name '*.c' ! -path 'kernel/arch/*' 2>/dev/null) \
            $(shell find $(KERNEL_ARCH_DIR) -name '*.c' 2>/dev/null)

ASM_SRCS := $(shell find $(BOOT_ASM_DIR) $(KERNEL_ARCH_DIR) -name '*.asm' 2>/dev/null) \
            $(shell find $(KERNEL_DIR) -name '*.asm' ! -path 'kernel/arch/*' 2>/dev/null)

C_OBJS   := $(patsubst %.c,   $(BUILD)/%.o, $(C_SRCS))
ASM_OBJS := $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))
OBJS     := $(ASM_OBJS) $(C_OBJS)

.PHONY: all iso iso-i686 iso-x86_64 debug run clean disk reset check format gdb

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

iso: iso-$(ARCH)

iso-i686: $(BUILD)/taavi.bin
	@$(MAKE) --no-print-directory -C userspace ARCH=$(ARCH) > /dev/null 2>&1 || true
	@mkdir -p isodir/boot/grub
	@cp $(BUILD)/taavi.bin isodir/boot/
	@cp userspace/build/bin/*.elf isodir/boot/ 2>/dev/null || true
	@cp grub.cfg isodir/boot/grub/grub.cfg
	@grub-mkrescue -o $(BUILD)/taavi.iso isodir > /dev/null 2>&1

iso-x86_64: $(BUILD)/taavi.bin
	@$(MAKE) --no-print-directory -C userspace ARCH=$(ARCH) > /dev/null 2>&1 || true
	@rm -rf iso_root
	@mkdir -p iso_root/boot/limine iso_root/EFI/BOOT
	@cp $(BUILD)/taavi.bin iso_root/boot/
	@cp userspace/build/bin/*.elf iso_root/boot/ 2>/dev/null || true
	@cp $(LIMINE_CONF) iso_root/boot/limine/limine.conf
	@cp $(LIMINE_DIR)/limine-bios.sys $(LIMINE_DIR)/limine-bios-cd.bin \
	    $(LIMINE_DIR)/limine-uefi-cd.bin iso_root/boot/limine/
	@cp $(LIMINE_DIR)/BOOTX64.EFI iso_root/EFI/BOOT/
	@xorriso -as mkisofs -R -r -J \
	    -b boot/limine/limine-bios-cd.bin \
	    -no-emul-boot -boot-load-size 4 -boot-info-table \
	    -hfsplus -apm-block-size 2048 \
	    --efi-boot boot/limine/limine-uefi-cd.bin \
	    -efi-boot-part --efi-boot-image --protective-msdos-label \
	    iso_root -o $(BUILD)/taavi.iso > /dev/null
	@$(LIMINE_DIR)/limine bios-install $(BUILD)/taavi.iso > /dev/null 2>&1

debug: iso
	@mkdir -p $(BUILD)
	@$(QEMU) $(QEMU_FW) \
		-drive file=$(BUILD)/taavi.iso,format=raw,if=ide,bus=0,unit=0,media=cdrom \
		-drive file=fat.img,format=raw,if=ide,bus=0,unit=1,media=disk \
		-boot d -serial stdio -no-reboot -no-shutdown -s -S

run: iso
	@mkdir -p $(BUILD)
	@$(QEMU) $(QEMU_FW) \
		-drive file=$(BUILD)/taavi.iso,format=raw,if=ide,bus=0,unit=0,media=cdrom \
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
