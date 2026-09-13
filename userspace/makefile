CC        = gcc
AS        = nasm
LIB_DIR   = library
BUILD     = build
BINDIR    = $(BUILD)/bin
LIBDIR    = $(BUILD)/lib

LIB_INC_FLAGS = $(shell find $(LIB_DIR) -type d | sed 's/^/-I/')

CFLAGS = -m32 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -O0 \
         -fno-pic -fno-pie -mno-accumulate-outgoing-args -mregparm=0 \
         -fno-stack-protector -Wall -Wextra -I../kernel/include/shared \
         $(LIB_INC_FLAGS)

LDFLAGS = -m32 -nostdlib -no-pie -T link.ld -Wl,--no-warn-rwx-segments

LIB_SRCS = $(shell find $(LIB_DIR) -type f -name '*.c')
LIB_OBJS = $(patsubst $(LIB_DIR)/%.c, $(LIBDIR)/%.o, $(LIB_SRCS))

START_OBJ = $(LIBDIR)/start.o

PROGRAMS = $(notdir $(shell find . -mindepth 1 -maxdepth 1 -type d -not -name '$(LIB_DIR)' -not -name '$(BUILD)'))
ELFS     = $(patsubst %, $(BINDIR)/%.elf, $(PROGRAMS))

all: $(ELFS)

$(LIBDIR)/%.o: $(LIB_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(START_OBJ): start.asm
	@mkdir -p $(LIBDIR)
	$(AS) -f elf32 $< -o $@

.SECONDEXPANSION:
$(BINDIR)/%.elf: $(BUILD)/%.o $(START_OBJ) $(LIB_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $(START_OBJ) $< $(LIB_OBJS)

$(BUILD)/%.o: $$(shell find % -type f -name '*.c')
	@mkdir -p $(BUILD)
	@mkdir -p $(BUILD)/$*-objs
	@for file in $^ ; do \
		obj="$(BUILD)/$*-objs/$$(echo $$file | tr '/' '_').o" ; \
		inc_dirs=$$(find $* -type d | sed 's/^/-I/'); \
		$(CC) $(CFLAGS) $$inc_dirs -c $$file -o $$obj ; \
	done
	ld -m elf_i386 -r $(BUILD)/$*-objs/*.o -o $@
	@rm -rf $(BUILD)/$*-objs

clean:
	rm -rf $(BUILD)

.PHONY: all clean
