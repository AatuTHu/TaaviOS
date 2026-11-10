#ifndef FB_H
#define FB_H

#include <stdint.h>

typedef struct {
    uint64_t phys_addr;
    uintptr_t virt_addr;
    uint32_t pitch;
    uint32_t width;
    uint32_t height;
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
uint32_t fb_pack_color(uint8_t r, uint8_t g, uint8_t b);
int fb_fill_rect(uint32_t *pixel_buffer, uint32_t x, uint32_t y, uint32_t rect_width, uint32_t rect_height, uint32_t stride, uint32_t buf_height, uint32_t color);
void fb_draw_string(uint32_t *pixel_buffer, uint32_t x, uint32_t y, uint32_t width, const char *str, uint32_t fg_color, uint32_t bg_color);
int fb_clear(uint32_t *pixel_buffer, uint32_t width, uint32_t height, uint32_t color);
int fb_scroll_down(uint32_t *pixel_buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t height, uint32_t stride, uint32_t color);
#endif
