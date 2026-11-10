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

.PHONY: iso
iso: $(BUILD)/taavi.bin
	@$(MAKE) --no-print-directory -C userspace ARCH=$(ARCH)
	@rm -rf iso_root
	@mkdir -p iso_root/boot/limine iso_root/EFI/BOOT
	@cp $(BUILD)/taavi.bin iso_root/boot/
	@cp $(USER_BIN)/*.elf iso_root/boot/
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
