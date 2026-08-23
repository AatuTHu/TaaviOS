#include "render.h"

/**
 * set_width - resize a window component's width.
 * @key: window component to modify.
 * @w: new width.
 *
 * Description:
 * Directly sets the width field on the given window component.
 *
 * Return: void.
 */
void set_width(uint32_t key, uint32_t w) {
    window_components[key]->width = w;
}

/**
 * set_height - resize a window component's height.
 * @key: window component to modify.
 * @h: new height.
 *
 * Description:
 * Directly sets the height field on the given window component.
 *
 * Return: void.
 */
void set_height(uint32_t key, uint32_t h) {
    window_components[key]->height = h;
}

/**
 * set_horizontal_padding - set left/right padding for a window component.
 * @key: window component to modify.
 * @hp: new horizontal padding, excluding border.
 *
 * Description:
 * Adds the component's existing border width to the requested padding,
 * so callers only need to think in terms of padding beyond the border.
 *
 * Return: void.
 */
void set_horizontal_padding(uint32_t key, uint32_t hp) {
    window_components[key]->def_horizontal_padding = hp + window_components[key]->border_width;
}

/**
 * set_vertical_padding - set top/bottom padding for a window component.
 * @key: window component to modify.
 * @vp: new vertical padding, excluding border.
 *
 * Description:
 * Adds the component's existing border width to the requested padding,
 * so callers only need to think in terms of padding beyond the border.
 *
 * Return: void.
 */
void set_vertical_padding(uint32_t key, uint32_t vp) {
    window_components[key]->def_vertical_padding = vp + window_components[key]->border_width;
}

/**
 * set_background_color - change a window component's background color.
 * @key: window component to modify.
 * @color: new background color.
 *
 * Description:
 * Directly sets the bg_color field on the given window component.
 *
 * Return: void.
 */
void set_background_color(uint32_t key, uint32_t color) {
    window_components[key]->bg_color = color;
}

/**
 * set_text_color - change a window component's foreground/text color.
 * @key: window component to modify.
 * @color: new foreground color.
 *
 * Description:
 * Directly sets the fg_color field on the given window component.
 *
 * Return: void.
 */
void set_text_color(uint32_t key, uint32_t color) {
    window_components[key]->fg_color = color;
}

/**
 * set_border_width - change a window component's border width.
 * @key: window component to modify.
 * @width: new border width.
 *
 * Description:
 * Directly sets the border_width field on the given window component.
 * Does not recompute existing padding, which is derived from border
 * width only at the time set_horizontal_padding/set_vertical_padding
 * are called.
 *
 * Return: void.
 */
void set_border_width(uint32_t key, uint32_t width) {
    window_components[key]->border_width = width;
}

/**
 * set_border_color - change a window component's border color.
 * @key: window component to modify.
 * @color: new border color.
 *
 * Description:
 * Directly sets the border_color field on the given window component.
 * Does not repaint the border; call clamp_borders_to_window
 * or paint_section to make the change visible.
 *
 * Return: void.
 */
void set_border_color(uint32_t key, uint32_t color) {
    window_components[key]->border_color = color;
}
