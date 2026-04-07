#include "klog.h"
#include "vga.h"
#include "serial.h"
#include <stdarg.h>

//start from zero dunno where go. Zero is the level where write to both vga and serial.
//Level one is for serial only
uint8_t LOG_LEVEL = 0;
uint8_t debug_mode = 0; //0 = false, 1 = true;

static void kputchar(char c) {
    if(LOG_LEVEL == 0 || LOG_LEVEL == 2) vga_putchar(c); 
    if(LOG_LEVEL != 2)      serial_putchar(c);
}

static void kputs(const char* s) {
    int i = 0;   
    while (s[i] != '\0') {
        kputchar(s[i]);
        i++;
    }
}

static void print_int(int n) {
    char buf[12];
    buf[11] = '\0';
    int pos = 10;

    if(n == 0) { 
        kputchar('0'); 
        return;
    }
    
    if (n < 0) {
        kputchar('-');
        n = -n;
    }

    while(n > 0) {
        buf[pos--] = '0' + (n % 10);
        n /= 10;
    }
    kputs(&buf[pos+1]);
}

static void print_hex(uint32_t n) {
    if(n == 0) { 
        kputchar('0');
        return; 
    }
    
    char digits[] = "0123456789abcdef";
    char buf[9];
    buf[8] = '\0';
    int pos = 7;
    
    while(n > 0) {
        buf[pos--] = digits[n % 16]; 
        n /= 16;
    }
    kputs(&buf[pos+1]);
}

static void kwrite (const char* fmt, va_list args) {
    for(int i = 0; fmt[i] != '\0'; i++) {
        if(fmt[i] != '%') {
            kputchar(fmt[i]);
        } else {
            i++;
            switch (fmt[i])
            {
            case 'd':
                print_int(va_arg(args, int));
                break;
            case 's':
                kputs(va_arg(args, char*));
                break;
            case 'c':
                kputchar((char)va_arg(args, int));
                break;
            case 'x':
                print_hex(va_arg(args, uint32_t));
                break;
            default:
                kputchar('%');
                break;
            }
        }
    }
}
void klog(const char* fmt, ...) {
    va_list args; //not important
    va_start(args, fmt); //not important
    kwrite(fmt,args); //main writer function call ->
    va_end(args); //not important
}

void DEBUG(const char* fmt, ...) {
    uint8_t last_level = LOG_LEVEL;
    if(debug_mode == 1) {
        set_log_level(1);
        va_list args; //not important
        va_start(args, fmt); //not important
        vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
        kputs("[DEBUG] ");
        vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
        kwrite(fmt,args); //main writer function call ->
        va_end(args); //not important
        set_log_level(last_level);
    }
}

//level 0 for vga and serial, level 1 for only serial, level 2 for only vga
void set_log_level(uint8_t level) {
    LOG_LEVEL = level;
}

void set_debug_mode() {
    if(debug_mode == 0) {
        debug_mode = 1;
    } else {
        debug_mode = 0;
    }
}