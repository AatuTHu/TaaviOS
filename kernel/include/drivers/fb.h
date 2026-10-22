#ifndef FB_H
#define FB_H

#include <stdint.h>

typedef struct {
    uintptr_t phys_addr;
    uintptr_t virt_addr;
    uintptr_t pitch;
    uintptr_t width;
    uintptr_t height;
    uint8_t bpp;
    uint8_t red_pos, red_size;
    uint8_t green_pos, green_size;
    uint8_t blue_pos, blue_size;
} fb_t;

extern fb_t fb;

#if defined(__x86_64__)
#include "boot_info.h"
int fb_init_limine(const struct boot_info *bi);
#else
#include "multiboot.h"
int fb_init_mboot(const struct multiboot_info *mbi);
#endif
uintptr_t fb_pack_color(uint8_t r, uint8_t g, uint8_t b);
int fb_fill_rect(uintptr_t *pixel_buffer, uintptr_t x, uintptr_t y, uintptr_t rect_width, uintptr_t rect_height, uintptr_t stride, uintptr_t buf_height, uintptr_t color);
void fb_draw_string(uintptr_t *pixel_buffer, uintptr_t x, uintptr_t y, uintptr_t width, const char *str, uintptr_t fg_color, uintptr_t bg_color);
int fb_clear(uintptr_t *pixel_buffer, uintptr_t width, uintptr_t height, uintptr_t color);
int fb_scroll_down(uintptr_t *pixel_buffer, uintptr_t x, uintptr_t y, uintptr_t width, uintptr_t height, uintptr_t stride, uintptr_t color);
#endif
