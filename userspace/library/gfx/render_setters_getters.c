#include "render.h"

void set_background_color(uint32_t color, uint32_t key) {
    window_components[key]->bg_color = color;
}

void set_text_color(uint32_t color, uint32_t key) {
    window_components[key]->fg_color = color;
}

void set_width(uint32_t w, uint32_t key) {
    window_components[key]->width = w;
}
void set_height(uint32_t h, uint32_t key) {
    window_components[key]->height = h;
}
void set_vertical_padding(uint32_t vp, uint32_t key) {
    window_components[key]->def_vertical_padding = vp + window_components[key]->border_width;
}

void set_horizontal_padding(uint32_t hp, uint32_t key) {
    window_components[key]->def_horizontal_padding = hp + window_components[key]->border_width;
}
