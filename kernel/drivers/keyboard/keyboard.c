#include "keyboard.h"
#include "isr.h"
#include "io.h"
#include "vga.h"
#include "sched.h"

static keyboard_buffer_t kbd_buf_instance;
static keyboard_buffer_t *keyboard_buffer = &kbd_buf_instance;
static int shift_pressed = 0;
static int caps_lock_pressed = 0;

void keyboard_irq_handler(void) {
    uint8_t scancode = inb(0x60);
    keyboard_handler(scancode);
}

void write_to_keyboard_buffer(char c) {
        if ((keyboard_buffer->write - keyboard_buffer->read) == KEYBOARD_BUFFER_SIZE) {
        return;
    }

    /* Store the byte at the wrapped index */
    keyboard_buffer->buf[keyboard_buffer->write % KEYBOARD_BUFFER_SIZE] = c;
    keyboard_buffer->write++;
}

int read_from_keyboard_buffer(char* out) {
    if (keyboard_buffer->read == keyboard_buffer->write) {
        return 0;
    }
    
    *out = keyboard_buffer->buf[keyboard_buffer->read % KEYBOARD_BUFFER_SIZE];
    keyboard_buffer->read++;
    
    return 1;
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
            write_to_keyboard_buffer(c);
            scheduler_wake_task(keyboard_buffer->foreground_pid);
        }
    }
}

int get_foreground_pid() {
    return keyboard_buffer->foreground_pid;
}

void set_foreground_pid(int pid) {
    keyboard_buffer->foreground_pid = pid;
}

void keyboard_init() {
    keyboard_buffer->read = 0;
    keyboard_buffer->write = 0;
    keyboard_buffer->foreground_pid = -1;
    irq_register_handler(1, keyboard_irq_handler);
}