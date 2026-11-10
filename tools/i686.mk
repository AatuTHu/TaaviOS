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

.PHONY: iso
iso: $(BUILD)/taavi.bin
	@$(MAKE) --no-print-directory -C userspace ARCH=$(ARCH)
	@mkdir -p isodir/boot/grub
	@cp $(BUILD)/taavi.bin isodir/boot/
	@cp $(USER_BIN)/*.elf isodir/boot/
	@cp grub.cfg isodir/boot/grub/grub.cfg
	@grub-mkrescue -o $(BUILD)/taavi.iso isodir > /dev/null 2>&1
