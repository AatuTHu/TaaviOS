CC = gcc
AS = nasm

# Polut
LIB_DIR = ../library
LIBC_DIR = $(LIB_DIR)/libc
SYS_DIR = $(LIB_DIR)/sys
BUILD = build

# Etsitään lähdekoodit näistä paikoista
VPATH = . : $(LIBC_DIR) : $(SYS_DIR)

# Huom: Lisää stand.c jos käytät sitä (kuten init-logissa näkyi)
CFLAGS = -m32 -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -O0 \
         -fno-pic -fno-pie -mno-accumulate-outgoing-args -mregparm=0 \
         -fno-stack-protector -Wall -Wextra \
         -I. -I$(LIBC_DIR) -I$(SYS_DIR)

LDFLAGS = -m32 -nostdlib -no-pie -T link.ld

# Määritä tarvittavat objektit. 
# Lisää listaan kaikki kirjastotiedostot, joita shell käyttää.
C_SRCS   = shell.c string.c sys_calls.c stand.c
ASM_SRCS = start.asm

# Luodaan objektilista ilman polkuja (notdir), jotta ne menevät build-kansioon
C_OBJS   = $(patsubst %.c, $(BUILD)/%.o, $(notdir $(C_SRCS)))
ASM_OBJS = $(patsubst %.asm, $(BUILD)/%.o, $(notdir $(ASM_SRCS)))
OBJS     = $(ASM_OBJS) $(C_OBJS)

all: $(BUILD)/shell.elf

$(BUILD)/%.o: %.c
	@mkdir -p $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.asm
	@mkdir -p $(BUILD)
	$(AS) -f elf32 $< -o $@

$(BUILD)/shell.elf: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)
	cp $(BUILD)/shell.elf ./shell.elf

clean:
	rm -rf $(BUILD)
	rm -f shell.elf

.PHONY: all clean