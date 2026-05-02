#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#define KEYBOARD_BUFFER_SIZE 256
// Scancode Set 1 → ASCII (Finnish/Scandinavian layout)
// Indeksi = scancode, arvo = ASCII-merkki (0 = ei merkkiä)
static const char scancode_to_ascii[] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '+', '\'', '\b',  // 0x00-0x0E
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', 0,   0,    '\n',     // 0x0F-0x1C
    0,   'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 0,   0,    0,    0,        // 0x1D-0x2A
    0,   'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '-', 0,    0,    0,        // 0x2B-0x38
    ' ',                                                                            // 0x39
};

static const char scancode_to_ascii_shifted[] = {
    0,   27,  '!', '"', '#', '$', '%', '&', '/', '(', ')', '=', '?', '`',  '\b',   // 0x00-0x0E
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0,   0,    '\n',     // 0x0F-0x1C
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0,   0,    0,    0,        // 0x1D-0x2A
    0,   'Z', 'X', 'C', 'V', 'B', 'N', 'M', ';', ':', '_', 0,    0,    0,        // 0x2B-0x38
    ' ',                                                                            // 0x39
};

static const char scancode_to_ascii_caps_lock[] = {
    0,   27,  '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '+', '\'', '\b',   // 0x00-0x0E
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', 0,   0,    '\n',     // 0x0F-0x1C
    0,   'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', 0,   0,    0,    0,        // 0x1D-0x2A
    0,   'Z', 'X', 'C', 'V', 'B', 'N', 'M', ',', '.', '-', 0,    0,    0,        // 0x2B-0x38
    ' ',                                                                            // 0x39
};

typedef struct keyboard_buffer_t {
    char buf[KEYBOARD_BUFFER_SIZE];
    volatile uint32_t read;
    volatile uint32_t write;
    int foreground_pid;
} keyboard_buffer_t;

void keyboard_init(void);
void keyboard_irq_handler(void);
void keyboard_handler(uint8_t scancode);
void write_to_keyboard_buffer(char c);
void set_foreground_pid(int pid);
int  read_from_keyboard_buffer(char* out);
int  get_foreground_pid();


#endif