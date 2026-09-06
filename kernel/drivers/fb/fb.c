#include "fb.h"
#include "config.h"
#include "font.h"
#include "klog.h"
#include "kstring.h"
#include "paging.h"
#include <stdint.h>

fb_t fb;

static void fb_put_pixel(uint32_t *pixel_buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t color) {
    uint32_t *position = (uint32_t *)(pixel_buffer + y * width + x);
    *position          = color;
}

uint32_t fb_pack_color(uint8_t r, uint8_t g, uint8_t b) {
    return (r << fb.red_pos) | (g << fb.green_pos) | (b << fb.blue_pos);
}

int fb_fill_rect(uint32_t *pixel_buffer, uint32_t x, uint32_t y, uint32_t rect_width, uint32_t rect_height, uint32_t stride, uint32_t buf_height, uint32_t color) {

    if (x + rect_width > stride || y + rect_height > buf_height) {
        ERROR("[FB][FILL_RECT]: x + width or y + height are higher than boundaries\n");
        return STATUS_ERROR;
    }

    for (uint32_t i = 0; i < rect_width; i++) {
        for (uint32_t j = 0; j < rect_height; j++) {
            fb_put_pixel(pixel_buffer, x + i, y + j, stride, color);
        }
    }

    return STATUS_OK;
}

int fb_clear(uint32_t *pixel_buffer, uint32_t width, uint32_t height, uint32_t color) {
    return fb_fill_rect(pixel_buffer, 0, 0, width, height, width, height, color);
}

int fb_scroll_down(uint32_t *pixel_buffer, uint32_t x, uint32_t y,
                   uint32_t width, uint32_t height, uint32_t stride,
                   uint32_t color) {
    if (pixel_buffer == NULL || height <= FONT_HEIGHT) {
        return STATUS_ERROR;
    }

    for (uint32_t row = 0; row < height - FONT_HEIGHT; row++) {
        memmove(
            pixel_buffer + (y + row) * stride + x,
            pixel_buffer + (y + row + FONT_HEIGHT) * stride + x,
            width * sizeof(uint32_t));
    }

    for (uint32_t row = height - FONT_HEIGHT; row < height; row++)
        for (uint32_t col = 0; col < width; col++)
            fb_put_pixel(pixel_buffer, x + col, y + row, stride, color);

    return STATUS_OK;
}

static void fb_draw_char(uint32_t *pixel_buffer, uint32_t x, uint32_t y, uint32_t width, char c, uint32_t fg_color, uint32_t bg_color) {
    uint8_t *glyph = (uint8_t *)PC_FACE_MODERNDOS_8x16[(uint8_t)c];

    if (c == '\0' || c == '\n' || c == '\t' || x >= width) {
        return;
    }

    for (int row = 0; row < FONT_HEIGHT; row++) {
        uint8_t byte = glyph[row];
        for (int col = 0; col < FONT_WIDTH; col++) {
            uint8_t bit    = byte & (0x80 >> col);
            uint32_t color = bit != 0 ? fg_color : bg_color;
            fb_put_pixel(pixel_buffer, x + col, y + row, width, color);
        }
    }
}

void fb_draw_string(uint32_t *pixel_buffer, uint32_t x, uint32_t y, uint32_t width, const char *str, uint32_t fg_color, uint32_t bg_color) {
    while (*str != '\0') {
        fb_draw_char(pixel_buffer, x, y, width, *str, fg_color, bg_color);
        x += FONT_WIDTH;
        str++;
    }
}

int fb_init(const struct multiboot_info *mbi) {
    DEBUG_FB("[FB] INITIALIZING FRAMEBUFFER GRAPHICS\n");
    if (!(mbi->flags & (1 << 12))) {
        ERROR("[FB] Invalid flags\n");
        return STATUS_ERROR;
    }

    fb.phys_addr        = mbi->framebuffer_addr;
    fb.width            = mbi->framebuffer_width;
    fb.height           = mbi->framebuffer_height;
    fb.pitch            = mbi->framebuffer_pitch;
    fb.bpp              = mbi->framebuffer_bpp;
    fb.virt_addr        = FB_VIRTUAL_BASE;

    fb.blue_pos         = mbi->framebuffer_blue_field_position;
    fb.red_pos          = mbi->framebuffer_red_field_position;
    fb.green_pos        = mbi->framebuffer_green_field_position;

    fb.blue_size        = mbi->framebuffer_blue_mask_size;
    fb.red_size         = mbi->framebuffer_red_mask_size;
    fb.green_size       = mbi->framebuffer_green_mask_size;

    uint32_t fb_size    = mbi->framebuffer_pitch * mbi->framebuffer_height;
    uint32_t page_count = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;
    paging_add_deferred_mapping(&kernel_page_dir, FB_VIRTUAL_BASE, fb.phys_addr, PAGE_PRESENT | PAGE_RW, page_count);
    DEBUG_FB("[FB]: framebuffer type: %d\n", mbi->framebuffer_type);
    DEBUG_FB("[FB]: framebuffer width: %d\n", fb.width);
    DEBUG_FB("[FB]: framebuffer height: %d\n", fb.height);
    DEBUG_FB("[FB]: framebuffer pitch: %d\n", fb.pitch);
    DEBUG_FB("[FB]: framebuffer bpp: %d\n", fb.bpp);
    DEBUG_FB("[FB]: framebuffer virt_addr: 0x%x\n", fb.virt_addr);
    DEBUG_FB("[FB]: framebuffer blue pos: %d\n", fb.blue_pos);
    DEBUG_FB("[FB]: framebuffer red pos: %d\n", fb.red_pos);
    DEBUG_FB("[FB]: framebuffer green pos: %d\n", fb.green_pos);
    DEBUG_FB("[FB]: framebuffer blue size: %d\n", fb.blue_size);
    DEBUG_FB("[FB]: framebuffer red size: %d\n", fb.red_size);
    DEBUG_FB("[FB]: framebuffer green size: %d\n", fb.green_size);

    return STATUS_OK;
}
