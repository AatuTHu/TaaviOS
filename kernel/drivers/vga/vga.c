#include "vga.h"
#include "io.h"
#include "config.h"

volatile unsigned short *vga = NULL;

/*
* This is shit should be rewritten, but it is the first things I wrote
*/


// Default: White (0x0F) on Black (0x00)
static uint8_t terminal_attribute = 0x0F; 

int x_pos = 0;
int y_pos = 0;

#define terminal_width 80
#define terminal_height 25


// Helper to get a "blank" character with the current color
static uint16_t get_blank_char() {
    return (uint16_t)terminal_attribute << 8 | ' ';
}

//calculates current cursos position
int get_terminal_pos() {
    return x_pos + (terminal_width * y_pos);
}


//calculates the "cross" position based on x and y co'oordinates
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

void clear_terminal() {
    x_pos = 0;
    y_pos = 0;
    uint16_t blank = get_blank_char();
    for (int i = 0; i < terminal_width * terminal_height; i++) {
        vga[i] = blank;
    }
    update_cursor();
}

// Helper to pack color bits
static uint8_t vga_create_attribute(enum vga_color fg, enum vga_color bg) {
    return (bg << 4) | (fg & 0x0F);
}

void vga_set_color(enum vga_color fg, enum vga_color bg) {
    terminal_attribute = vga_create_attribute(fg, bg);
}

void vga_init() {
    x_pos = 0;
    y_pos = 0;
    vga = (volatile unsigned short *)VGA_MEMORY_ADDRESS; //hardcode hack
    //clear_terminal();
}

void lift_texts_up() {
    // Copy rows up
    for(int y = 1; y < terminal_height; y++) {
        for(int x = 0; x < terminal_width; x++) {
            vga[get_pos(x, y-1)] = vga[get_pos(x, y)];
        }
    }  

    // Clear the last line with the current color
    uint16_t blank = get_blank_char();
    for(int x = 0; x < terminal_width; x++) {
        vga[get_pos(x, 24)] = blank;
    }
    
    y_pos = 23;
}

void backspace_pressed() {
    if (x_pos > 0) {
        x_pos--;
    } else if (y_pos > 0) {
        y_pos--;
        x_pos = terminal_width - 1;
    } else {
        return; // At 0,0 - nothing to do
    }
    
    vga[get_terminal_pos()] = get_blank_char(); 
    update_cursor();
}

void vga_putchar(char c) {
    if(y_pos >= 25) lift_texts_up();

    switch (c) {
    case '\n':
        x_pos = 0;
        y_pos++;
        break;
    case '\b':
        backspace_pressed();
        return;
    case '\t':
        x_pos = (x_pos + 8) & ~7; // Standard 8-space tabs
        if(x_pos >= 80) {
            y_pos++;
            x_pos = 0;
        }
        break;
    default: 
        // Combine attribute (high) and character (low)
        vga[get_terminal_pos()] = (uint16_t)terminal_attribute << 8 | (unsigned char)c;
        x_pos++;
        
        if(x_pos >= 80) {
            x_pos = 0;
            y_pos++;
        }
        break;
    }

    // Check again if the newline/wrap pushed us off screen
    if(y_pos >= 25) lift_texts_up();
    
}

void vga_write(char *msg) {
    for (int i = 0; msg[i] != '\0'; i++) {
        vga_putchar(msg[i]);
    }
    update_cursor();
}