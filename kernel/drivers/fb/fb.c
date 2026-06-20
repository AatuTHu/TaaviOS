#include "fb.h"
#include "config.h"
#include "font.h"
#include "klog.h"
#include "mm.h"
#include "paging.h"
#include "vmm.h"

fb_t fb;
static uint32_t fb_size    = 0;
static uint32_t page_count = 0;

static void fb_put_pixel(uint32_t x, uint32_t y, uint32_t color) {
    uint32_t *position = (uint32_t *)(fb.virt_addr + y * fb.pitch + x * (fb.bpp / 8));
    *position          = color;
    // DEBUG("[FB][PUT_PIXEL]: position: %d\n", *position);
}

uint32_t fb_pack_color(uint8_t r, uint8_t g, uint8_t b) {
    return (r << fb.red_pos) | (g << fb.green_pos) | (b << fb.blue_pos);
}

void fb_fill_rect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {

    if (x >= fb.width || y >= fb.height) {
        ERROR("[FB][FILL_RECT]: Bad coo'oordinates. Aborting\n");
        return;
    }

    for (uint32_t i = 0; i < w; i++) {
        if (x + i >= fb.width)
            break;
        for (uint32_t j = 0; j < h; j++) {
            if (y + j >= fb.height)
                break;
            fb_put_pixel(x + i, y + j, color);
        }
    }
}

void fb_clear(uint32_t color) {
    fb_fill_rect(0, 0, fb.width, fb.height, color);
}

// y font height
// x space between chars?
static void fb_draw_char(uint32_t **x, uint32_t **y, unsigned char c, uint32_t fg_color, uint32_t bg_color) {
    uint8_t *glyph = PC_FACE_MODERNDOS_8x16[c];

    switch (c) {
    case '\0':
        return;
    case '\n':
        **y += 16;
        **x = 0;
        return;
    case '\b':
        if (**x >= 8) {
            **x -= 8;
        } else if (**y >= 16) {
            **y -= 16;
            **x = fb.width - 8;
        } else if (**x == 0 && **y == 0) {
            return;
        }
        glyph = PC_FACE_MODERNDOS_8x16[0];
        for (int row = 0; row < 16; row++) {
            uint8_t byte = glyph[row];
            for (int col = 0; col < 8; col++) {
                uint8_t bit    = byte & (0x80 >> col);
                uint32_t color = bit != 0 ? fg_color : bg_color;
                fb_put_pixel(**x + col, **y + row, color);
            }
        }
        return;
    }

    for (int row = 0; row < 16; row++) {
        uint8_t byte = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint8_t bit    = byte & (0x80 >> col);
            uint32_t color = bit != 0 ? fg_color : bg_color;
            fb_put_pixel(**x + col, **y + row, color);
        }
    }

    **x += 8;
    if (**x >= fb.width) {
        **y += 16;
        **x = 0;
    }
}

void fb_draw_string(uint32_t *x, uint32_t *y, const char *str, uint32_t fg_color, uint32_t bg_color) {
    while (*str != '\0') {
        fb_draw_char(&x, &y, *str, fg_color, bg_color);
        str++;
    }
}

// Workaround
void __fb_map_page() {
    DEBUG("[FB][MAP_FB_PAGE]: Mapping framebuffer\n");
    for (uint32_t i = 0; i < page_count; i++) {
        paging_map(&kernel_page_dir, FB_VIRTUAL_BASE + i * PAGE_SIZE, fb.phys_addr + i * PAGE_SIZE, PAGE_PRESENT | PAGE_RW);
    }
    DEBUG("[FB][MAP_FB_PAGE]: Mapping successfull\n");
}

int fb_init(const struct multiboot_info *mbi) {
    DEBUG("[FB] INITIALIZING FRAMEBUFFER GRAPHICS\n");
    if (!(mbi->flags & (1 << 12))) {
        ERROR("[FB] Invalid flags\n");
        return STATUS_ERROR;
    }
    DEBUG("[FB] Mapping page for framebuffer\n");
    DEBUG("[FB]: mbi framebuffer addr: 0x%x\n", mbi->framebuffer_addr);

    fb_size    = mbi->framebuffer_pitch * mbi->framebuffer_height;
    page_count = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;

    DEBUG("[FB]: framebuffer flags: %d\n", mbi->flags);
    DEBUG("[FB]: fb_size: %d\n", fb_size);
    DEBUG("[FB]: page_count: %d\n", page_count);
    /*for (uint32_t i = 0; i < page_count; i++) {
        paging_map(&kernel_page_dir, FB_VIRTUAL_BASE + i * PAGE_SIZE, (uint32_t)mbi->framebuffer_addr + i * PAGE_SIZE, PAGE_PRESENT | PAGE_RW);
    }*/
    fb.phys_addr = mbi->framebuffer_addr;
    fb.width     = mbi->framebuffer_width;
    fb.height    = mbi->framebuffer_height;
    fb.pitch     = mbi->framebuffer_pitch;
    fb.bpp       = mbi->framebuffer_bpp;
    fb.virt_addr = FB_VIRTUAL_BASE;

    fb.blue_pos  = mbi->framebuffer_blue_field_position;
    fb.red_pos   = mbi->framebuffer_red_field_position;
    fb.green_pos = mbi->framebuffer_green_field_position;

    fb.blue_size  = mbi->framebuffer_blue_mask_size;
    fb.red_size   = mbi->framebuffer_red_mask_size;
    fb.green_size = mbi->framebuffer_green_mask_size;

    DEBUG("[FB]: framebuffer type: %d\n", mbi->framebuffer_type);
    DEBUG("[FB]: framebuffer width: %d\n", fb.width);
    DEBUG("[FB]: framebuffer height: %d\n", fb.height);
    DEBUG("[FB]: framebuffer pitch: %d\n", fb.pitch);
    DEBUG("[FB]: framebuffer bpp: %d\n", fb.bpp);
    DEBUG("[FB]: framebuffer virt_addr: 0x%x\n", fb.virt_addr);
    DEBUG("[FB]: framebuffer blue pos: %d\n", fb.blue_pos);
    DEBUG("[FB]: framebuffer red pos: %d\n", fb.red_pos);
    DEBUG("[FB]: framebuffer green pos: %d\n", fb.green_pos);
    DEBUG("[FB]: framebuffer blue size: %d\n", fb.blue_size);
    DEBUG("[FB]: framebuffer red size: %d\n", fb.red_size);
    DEBUG("[FB]: framebuffer green size: %d\n", fb.green_size);

    return STATUS_OK;
}