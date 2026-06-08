#include "serial.h"
#include "io.h"

#define COM1 0x3F8

/*
 * Most generic serial thingy
 */

static int serial_is_ready(void) {
    return inb(COM1 + 5) & 0x20;
}

uint8_t serial_received() {
    return inb(COM1 + 5) & 1;
}

void serial_init(void) {
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x80);
    outb(COM1 + 0, 0x03);
    outb(COM1 + 1, 0x00);
    outb(COM1 + 3, 0x03);
    outb(COM1 + 2, 0xC7);
    outb(COM1 + 4, 0x0B);
}

void serial_putchar(char c) {
    while (serial_is_ready() == 0);
    outb(COM1, c);
}

void serial_write(const char *str) {
    int i = 0;
    while (str[i] != '\0') {
        serial_putchar(str[i]);
        i++;
    }
}