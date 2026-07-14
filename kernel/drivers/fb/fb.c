#include "fb.h"
#include "config.h"
#include "font.h"
#include "klog.h"
#include "kstring.h"
#include "mm.h"
#include "paging.h"
#include "vmm.h"

fb_t fb;
static uint32_t fb_size    = 0;
static uint32_t page_count = 0;

static void fb_put_pixel(uint32_t *pixel_buffer, uint32_t x, uint32_t y, uint32_t width, uint32_t color) {
    uint32_t *position = (uint32_t *)(pixel_buffer + y * width + x);
    // DEBUG_FB("[FB][PUT_PIXEL]: position: %d\n", *position);
    *position = color;
}

uint32_t fb_pack_color(uint8_t r, uint8_t g, uint8_t b) {
    return (r << fb.red_pos) | (g << fb.green_pos) | (b << fb.blue_pos);
}

void fb_fill_rect(uint32_t *pixel_buffer, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint32_t color) {

    /*
    DEBUG_FB("[FB][FILL_REXT]: given buffer: 0x%x\n", pixel_buffer);
    DEBUG_FB("[FB][FILL_RECT]: given x %d\n", x);
    DEBUG_FB("[FB][FILL_RECT]: given y %d\n", y);
    DEBUG_FB("[FB][FILL_RECT]: given w %d\n", w);
    DEBUG_FB("[FB][FILL_RECT]: given h %d\n", h);
    DEBUG_FB("[FB][FILL_RECT]: given color %d\n", color);
    DEBUG_FB("[FB][FILL_RECT]: fb width %d\n", fb.width);
    DEBUG_FB("[FB][FILL_RECT]: fb height %d\n", fb.height);*/

    if (w > fb.width || h > fb.height) {
        ERROR("[FB][FILL_RECT]: Window cant be biger than the screen\n");
        return;
    }

    for (uint32_t i = 0; i < w; i++) {
        if ((x + i) >= fb.width) {
            DEBUG_FB("[FB][FILL_RECT]: exceeded screen width x: %d & i: %d & width: %d\n", x, i, fb.width);
            break;
        }

        for (uint32_t j = 0; j < h; j++) {
            if ((y + j) >= fb.height) {
                DEBUG_FB("[FB][FILL_REXT]: exceeded screen height y: %d & j: %d & height: %d\n", y, j, fb.height);
                break;
            }

            fb_put_pixel(pixel_buffer, x + i, y + j, w, color);
        }
    }
}

void fb_clear(uint32_t *pixel_buffer, uint32_t width, uint32_t height, uint32_t color) {
    DEBUG_FB("[FB][CLEAR_WINDOW]: clear width: %d\n", width);
    DEBUG_FB("[FB][CLEAR_WINDOW]: clear height: %d\n", height);
    fb_fill_rect(pixel_buffer, 0, 0, width, height, color);
}

// y font height
// x space between chars?
static void fb_draw_char(uint32_t *pixel_buffer, uint32_t **x, uint32_t **y, uint32_t width, uint32_t height, char c, uint32_t fg_color, uint32_t bg_color) {
    const uint8_t *glyph = PC_FACE_MODERNDOS_8x16[(uint8_t)c];

    switch (c) {
    case '\0':
        return;
    case '\n':
        if ((**y + 16) > height) {
            uint32_t *buf = (uint32_t *)pixel_buffer;
            memmove(buf, buf + width * 16, (height - 16) * width * sizeof(uint32_t));
            for (uint32_t row = height - 16; row < height; row++)
                for (uint32_t col = 0; col < width; col++)
                    fb_put_pixel(pixel_buffer, col, row, width, bg_color);
            **y = height - 16;
            **x = 0;
            return;
        }

        **y += 16;
        **x = 0;
        return;
    case '\b':
        if (**x >= 8) {
            **x -= 8;
        } else if (**y >= 16) {
            **y -= 16;
            **x = width - 8;
        } else if (**x == 0 && **y == 0) {
            return;
        }
        const uint8_t *empty = PC_FACE_MODERNDOS_8x16[0];
        for (int row = 0; row < 16; row++) {
            uint8_t byte = empty[row];
            for (int col = 0; col < 8; col++) {
                uint8_t bit    = byte & (0x80 >> col);
                uint32_t color = bit != 0 ? fg_color : bg_color;
                fb_put_pixel(pixel_buffer, **x + col, **y + row, width, color);
            }
        }
        return;
    }

    for (int row = 0; row < 16; row++) {
        uint8_t byte = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint8_t bit    = byte & (0x80 >> col);
            uint32_t color = bit != 0 ? fg_color : bg_color;
            fb_put_pixel(pixel_buffer, **x + col, **y + row, width, color);
        }
    }

    **x += 8;

    if ((**y + 16) > height) {
        uint32_t *buf = (uint32_t *)pixel_buffer;
        memmove(buf, buf + width * 16, (height - 16) * width * sizeof(uint32_t));
        for (uint32_t row = height - 16; row < height; row++)
            for (uint32_t col = 0; col < width; col++)
                fb_put_pixel(pixel_buffer, col, row, width, bg_color);
        **y = height - 16;
        **x = 0;
        return;
    }

    if (**x >= width) {
        **y += 16;
        **x = 0;
    }
}

void fb_draw_string(uint32_t *pixel_buffer, uint32_t *x, uint32_t *y, uint32_t width, uint32_t height, const char *str, uint32_t fg_color, uint32_t bg_color) {
    while (*str != '\0') {
        fb_draw_char(pixel_buffer, &x, &y, width, height, *str, fg_color, bg_color);
        str++;
    }
}

// Workaround
void __fb_map_page() {
    DEBUG_FB("[FB][MAP_FB_PAGE]: Mapping framebuffer\n");
    for (uint32_t i = 0; i < page_count; i++) {
        paging_map(&kernel_page_dir, FB_VIRTUAL_BASE + i * PAGE_SIZE, fb.phys_addr + i * PAGE_SIZE, PAGE_PRESENT | PAGE_RW);
    }
    DEBUG_FB("[FB][MAP_FB_PAGE]: Mapping successfull\n");
}

int fb_init(const struct multiboot_info *mbi) {
    DEBUG_FB("[FB] INITIALIZING FRAMEBUFFER GRAPHICS\n");
    if (!(mbi->flags & (1 << 12))) {
        ERROR("[FB] Invalid flags\n");
        return STATUS_ERROR;
    }
    DEBUG_FB("[FB] Mapping page for framebuffer\n");
    DEBUG_FB("[FB]: mbi framebuffer addr: 0x%x\n", mbi->framebuffer_addr);

    fb_size    = mbi->framebuffer_pitch * mbi->framebuffer_height;
    page_count = (fb_size + PAGE_SIZE - 1) / PAGE_SIZE;

    DEBUG_FB("[FB]: framebuffer flags: %d\n", mbi->flags);
    DEBUG_FB("[FB]: fb_size: %d\n", fb_size);
    DEBUG_FB("[FB]: page_count: %d\n", page_count);
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