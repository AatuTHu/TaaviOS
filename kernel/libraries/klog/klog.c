#include "klog.h"
#include "serial.h"
#include "vga.h"
#include <stdarg.h>

/*
 * Good enough
 */

static void kputchar(char c) {
    serial_putchar(c);
}

static void kputs(const char *s) {
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

    if (n == 0) {
        kputchar('0');
        return;
    }

    if (n < 0) {
        kputchar('-');
        n = -n;
    }

    while (n > 0) {
        buf[pos--] = '0' + (n % 10);
        n /= 10;
    }
    kputs(&buf[pos + 1]);
}

static void print_hex(uint32_t n) {
    if (n == 0) {
        kputchar('0');
        return;
    }

    const char digits[] = "0123456789abcdef";
    char buf[9];
    buf[8]  = '\0';
    int pos = 7;

    while (n > 0) {
        buf[pos--] = digits[n % 16];
        n /= 16;
    }
    kputs(&buf[pos + 1]);
}

static void kputs_n(const char *s, int n) {
    for (int i = 0; i < n; i++) {
        if (s[i] == '\0')
            return;
        kputchar(s[i]);
    }
}

static void kwrite(const char *fmt, va_list args) {
    for (int i = 0; fmt[i] != '\0'; i++) {
        if (fmt[i] != '%') {
            kputchar(fmt[i]);
        } else {
            i++;
            int precision = -1;
            if (fmt[i] == '.') {
                i++;
                precision = 0;
                while (fmt[i] >= '0' && fmt[i] <= '9') {
                    precision = precision * 10 + (fmt[i] - '0');
                    i++;
                }
            }

            switch (fmt[i]) {
            case 'd':
                print_int(va_arg(args, int));
                break;
            case 's':
                if (precision >= 0)
                    kputs_n(va_arg(args, char *), precision);
                else
                    kputs(va_arg(args, char *));
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
void klog(const char *fmt, ...) {
    va_list args;        // not important
    va_start(args, fmt); // not important
    vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    kputs("[OK]: ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kwrite(fmt, args); // main writer function call ->
    va_end(args);      // not important
}

void klog_debug(const char *fmt, ...) {

    va_list args;        // not important
    va_start(args, fmt); // not important
    vga_set_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK);
    // kputs("[DEBUG] ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kwrite(fmt, args); // main writer function call ->
    va_end(args);      // not important
}

void klog_error(const char *fmt, ...) {

    va_list args;        // not important
    va_start(args, fmt); // not important
    vga_set_color(VGA_COLOR_RED, VGA_COLOR_BLACK);
    kputs("\n[ERROR] ");
    vga_set_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK);
    kwrite(fmt, args); // main writer function call ->
    kputs("\n");
    va_end(args); // not important
}
