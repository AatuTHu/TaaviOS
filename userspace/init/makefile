CC = gcc
AS = nasm
CFLAGS = -m32 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -O0 \
         -fno-pic -fno-pie -mno-accumulate-outgoing-args -mregparm=0 \
         -fno-stack-protector -Wall -Wextra
LDFLAGS = -m32 -nostdlib -no-pie -T link.ld

BUILD = build

C_SRCS   = main.c $(LIBC_SRCS)
ASM_SRCS = start.asm

C_OBJS   = $(patsubst %.c, $(BUILD)/%.o, $(C_SRCS))
ASM_OBJS = $(patsubst %.asm, $(BUILD)/%.o, $(ASM_SRCS))
OBJS     = $(ASM_OBJS) $(C_OBJS)

all: $(BUILD)/init.elf

$(BUILD)/%.o: %.asm
	mkdir -p $(dir $@)
	$(AS) -f elf32 $< -o $@

$(BUILD)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/init.elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	cp $(BUILD)/init.elf ./init.elf

clean:
	rm -rf $(BUILD)
	rm -f init.elf

.PHONY: all clean