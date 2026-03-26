#include "vga.h"
#include "io.h"
#include "config.h"

volatile unsigned short *vga = NULL;

int x_pos = 0;
int y_pos = 0;

#define terminal_width 80
#define terminal_height 25
#define color_white_on_black 0x0f20 

int get_terminal_pos() {
    return x_pos + (terminal_width * y_pos);
}

int get_pos(int x, int y) {
    return x + (terminal_width * y); 
}

void update_cursor() {
    uint16_t pos = get_terminal_pos();
    
    outb(0x3D4, 0x0E);
    outb(0x3D5, (pos >> 8) & 0xFF);
    
    outb(0x3D4, 0x0F);
    outb(0x3D5, pos & 0xFF);
}

void vga_init() {
    x_pos = 0;
    y_pos = 0;
    vga = (volatile unsigned short *)VGA_MEMORY_ADDRESS;
    for (int i = 0; i < terminal_width * terminal_height; i++) {
        vga[i] = color_white_on_black;
    }
    update_cursor();
}

void clear_terminal() {
    x_pos = 0;
    y_pos = 0;
    for (int i = 0; i < terminal_width * terminal_height; i++) {
        vga[i] = color_white_on_black;
    }
    update_cursor();
}

void lift_texts_up() {
    for(int y = 1; y < terminal_height; y++) {
        for(int x = 0; x < terminal_width; x++) {
            vga[get_pos(x, y-1)] = vga[get_pos(x, y)];
        }
    }  

    for(int x = 0; x < terminal_width; x++) {
        vga[get_pos(x, 24)] = color_white_on_black;
    }
    
    y_pos = 23;
}

void backspace_pressed() {
    if(y_pos == 0 || x_pos == 0) { return; }
    x_pos--;
    vga[get_terminal_pos()] = color_white_on_black; 
    update_cursor();
}

void vga_putchar(char c) { //This might be removed from here. Kinda not what vga is supposed to do. But hmm
    if(y_pos == 24) lift_texts_up();

    switch (c)
    {
    case '\n':
        x_pos = 0;
        y_pos++;
        break;
    case '\b':
        backspace_pressed();
        return;
    case '\t':
        x_pos = (x_pos + 4) & ~3;
        if(x_pos >= 80) {
            y_pos++;
            x_pos = 0;
        }
        break;
    default: 
        vga[get_terminal_pos()] = (0x0F << 8) | (unsigned char)c;
        x_pos++;
        
        if(x_pos >= 80) {
            x_pos = 0;
            y_pos++;
        }
        break;
    }

    update_cursor();
}

void vga_write(char *msg) { //loop through message until end. Removes the need for needing to know how long of a array to handle.
    for (int i = 0; msg[i] != '\0'; i++) {
        vga_putchar(msg[i]);
    }
}