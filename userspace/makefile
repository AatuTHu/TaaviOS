CC     = gcc
AS     = nasm
LIB_DIR = library
LIBC_DIR = $(LIB_DIR)/libc
SYS_DIR  = $(LIB_DIR)/sys
BUILD    = build
BINDIR   = $(BUILD)/bin
LIBDIR   = $(BUILD)/lib

CFLAGS = -m32 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -O0 \
         -fno-pic -fno-pie -mno-accumulate-outgoing-args -mregparm=0 \
         -fno-stack-protector -Wall -Wextra -I../kernel/include/shared\
         -I$(LIBC_DIR) \
         -I$(SYS_DIR)

LDFLAGS = -m32 -nostdlib -no-pie -T link.ld

LIBC_SRCS = $(wildcard $(LIBC_DIR)/*.c)
SYS_SRCS  = $(wildcard $(SYS_DIR)/*.c)

LIBC_OBJS = $(patsubst $(LIBC_DIR)/%.c, $(LIBDIR)/libc/%.o, $(LIBC_SRCS))
SYS_OBJS  = $(patsubst $(SYS_DIR)/%.c, $(LIBDIR)/sys/%.o, $(SYS_SRCS))
LIB_OBJS  = $(LIBC_OBJS) $(SYS_OBJS)

START_OBJ = $(LIBDIR)/start.o

PROGRAMS = $(notdir $(shell find . -mindepth 1 -maxdepth 1 -type d \
           -not -name 'library' -not -name 'build'))
ELFS     = $(patsubst %, $(BINDIR)/%.elf, $(PROGRAMS))

all: $(ELFS)

$(LIBDIR)/libc/%.o: $(LIBC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(LIBDIR)/sys/%.o: $(SYS_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(START_OBJ): start.asm
	@mkdir -p $(LIBDIR)
	$(AS) -f elf32 $< -o $@

.SECONDEXPANSION:
$(BINDIR)/%.elf: $(BUILD)/%.o $(START_OBJ) $(LIB_OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(LDFLAGS) -o $@ $(START_OBJ) $< $(LIB_OBJS)

$(BUILD)/%.o: $$(wildcard %/*.c)
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -I$* -c $< -o $@

clean:
	rm -rf $(BUILD)

.PHONY: all clean