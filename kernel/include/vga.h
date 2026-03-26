#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdint.h>
#include <stddef.h>

void vga_init(void);
void vga_putchar(char c);
void vga_write(char *msg);

#endif