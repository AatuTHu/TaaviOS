ARCH ?= i686

CC        = gcc
AS        = nasm
LIB_DIR   = library

ifeq ($(ARCH),x86_64)
    ARCH_CFLAGS  = -m64 -mno-red-zone -mno-sse -mno-sse2 -mno-mmx
    ARCH_LDFLAGS = -m64 -z max-page-size=0x1000
    ASFLAGS      = -f elf64
    LD_EMU       = elf_x86_64
    LINK_SCRIPT  = _boot/link_x86_64.ld
    START_SRC    = _boot/start_x86_64.asm
else ifeq ($(ARCH),i686)
    ARCH_CFLAGS  = -m32 -mno-accumulate-outgoing-args -mregparm=0
    ARCH_LDFLAGS = -m32
    ASFLAGS      = -f elf32
    LD_EMU       = elf_i386
    LINK_SCRIPT  = _boot/link_i686.ld
    START_SRC    = _boot/start_i686.asm
else
    $(error Unsupported ARCH $(ARCH). Use ARCH=x86_64 or ARCH=i686)
endif

BUILD     = build/$(ARCH)
BINDIR    = $(BUILD)/bin
LIBDIR    = $(BUILD)/lib

LIB_INC_FLAGS = $(shell find $(LIB_DIR) -type d | sed 's/^/-I/')

CFLAGS = $(ARCH_CFLAGS) -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -O0 \
         -fno-pic -fno-pie -fno-stack-protector -Wall -Wextra \
         -I../kernel/include/shared $(LIB_INC_FLAGS)

LDFLAGS = $(ARCH_LDFLAGS) -nostdlib -no-pie -T $(LINK_SCRIPT) -Wl,--no-warn-rwx-segments

LIB_SRCS = $(shell find $(LIB_DIR) -type f -name '*.c')
LIB_OBJS = $(patsubst $(LIB_DIR)/%.c, $(LIBDIR)/%.o, $(LIB_SRCS))

START_OBJ = $(LIBDIR)/start.o

PROGRAMS = $(notdir $(shell find . -mindepth 1 -maxdepth 1 -type d \
            -not -name '.*' \
            -not -name '$(LIB_DIR)' \
            -not -name 'build' \
            -not -name '_boot' \
            -not -name 'kernel' \
            -not -name 'iso_root'))
ELFS     = $(patsubst %, $(BINDIR)/%.elf, $(PROGRAMS))

all: $(ELFS)

$(LIBDIR)/%.o: $(LIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(START_OBJ): $(START_SRC)
	@mkdir -p $(LIBDIR)
	$(AS) $(ASFLAGS) $< -o $@

.SECONDEXPANSION:
$(BINDIR)/%.elf: $(BUILD)/%.o $(START_OBJ) $(LIB_OBJS) $(LINK_SCRIPT)
	@mkdir -p $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $(START_OBJ) $< $(LIB_OBJS)

$(BUILD)/%.o: $$(shell find % -type f -name '*.c')
	@mkdir -p $(BUILD)
	@mkdir -p $(BUILD)/$*-objs
	@if [ -n "$^" ]; then \
		for file in $^ ; do \
			obj="$(BUILD)/$*-objs/$$(echo $$file | tr '/' '_').o" ; \
			inc_dirs=$$(find $* -type d | sed 's/^/-I/'); \
			$(CC) $(CFLAGS) $$inc_dirs -c $$file -o $$obj ; \
		done; \
		ld -m $(LD_EMU) -r $(BUILD)/$*-objs/*.o -o $@; \
	fi
	@rm -rf $(BUILD)/$*-objs

clean:
	@rm -rf build

.PHONY: all clean
