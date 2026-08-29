#include "ui.h"
#include "font.h"
#include "log.h"
#include "render.h"
#include "shared.h"
#include "stand.h"
#include "string.h"
#include <stdint.h>

int create_button(uint32_t width, uint32_t height, uint32_t x, uint32_t y, const char *title) {

    if (title == NULL) {
        return STATUS_ERROR;
    }

    uint32_t title_width = strlen(title) * FONT_WIDTH;

    if (width < title_width) {
        width = title_width;
    }

    if (height < FONT_HEIGHT) {
        height = FONT_HEIGHT;
    }

    int button_id = gfx_register_region(x, y, width, height, COLOR_WHITE, COLOR_DEEP_BLUE);

    if (button_id == STATUS_ERROR) {
        return STATUS_ERROR;
    }

    gfx_region_t *button = gfx_regions[button_id];

    gfx_clamp_horizontal(button, gfx_regions[PRIMARY_VIEWPORT_ID]);
    gfx_clamp_vertical(button, gfx_regions[PRIMARY_VIEWPORT_ID]);

    button->cursor_x += (button->width / 2) - ((strlen(title) * FONT_WIDTH / 2));
    button->cursor_y += (button->height - FONT_HEIGHT) / 2;

    gfx_draw_text_at(button_id, button->cursor_x, button->cursor_y, title);

    return STATUS_OK;
}
