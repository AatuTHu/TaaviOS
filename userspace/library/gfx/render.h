#ifndef RENDER_H
#define RENDER_H
#include <stdint.h>

int render(const char *text, uint32_t len);
void register_dimensions(uint32_t w, uint32_t h);
void set_dimensions(uint32_t w, uint32_t h);
void set_vertical_padding(uint32_t vp);
void set_horizontal_padding(uint32_t hp);
void init_renderer(uint32_t w, uint32_t h);

#endif
