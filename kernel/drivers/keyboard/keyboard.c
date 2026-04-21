#include "keyboard.h"
#include "isr.h"
#include "io.h"
#include "vga.h"

static int shift_pressed = 0;
static int caps_lock_pressed = 0;


void keyboard_irq_handler(void) {
    uint8_t scancode = inb(0x60);
    keyboard_handler(scancode);
}

void keyboard_init() {
    irq_register_handler(1, keyboard_irq_handler);
}

void keyboard_handler(uint8_t scancode) {
    if ((scancode == 0x2A) || (scancode == 0x36)) { shift_pressed = 1; return; }
    if ((scancode == 0xAA) || (scancode == 0xB6)) { shift_pressed = 0; return; }
    
    if (scancode == 0x3A) {caps_lock_pressed = 1 - caps_lock_pressed; return;}
   
    if ((scancode < sizeof(scancode_to_ascii) && !(scancode & 0x80))) {
        char c = shift_pressed ? scancode_to_ascii_shifted[scancode] : scancode_to_ascii[scancode];
      
        if (caps_lock_pressed) {
            c = scancode_to_ascii_caps_lock[scancode];
        }
       
        if (caps_lock_pressed && shift_pressed) {
            c = scancode_to_ascii[scancode];
        }
      
        if (c != 0) {
            vga_putchar(c);
        }
    }
}