#include "render.h"

/**
 * gfx_set_width - resize a region's width.
 * @region_id: region component to modify.
 * @w: new width.
 *
 * Description:
 * Directly sets the width field on the given region component.
 *
 * Return: void.
 */
void gfx_set_width(uint32_t region_id, uint32_t w) {
    gfx_regions[region_id]->width = w;
}

/**
 * gfx_set_height - resize a region's height.
 * @region_id: region component to modify.
 * @h: new height.
 *
 * Description:
 * Directly sets the height field on the given region component.
 *
 * Return: void.
 */
void gfx_set_height(uint32_t region_id, uint32_t h) {
    gfx_regions[region_id]->height = h;
}

/**
 * gfx_set_padding_x - set left/right padding for a region.
 * @region_id: region component to modify.
 * @px: new horizontal padding, excluding border.
 *
 * Description:
 * Adds the component's existing border width to the requested padding,
 * so callers only need to think in terms of padding beyond the border.
 *
 * Return: void.
 */
void gfx_set_padding_x(uint32_t region_id, uint32_t px) {
    if (region_id == PRIMARY_VIEWPORT_ID) {
        gfx_regions[region_id]->padding_x = px + gfx_regions[region_id]->border_width;
        return;
    }
    gfx_region_t *parent              = gfx_regions[PRIMARY_VIEWPORT_ID];
    gfx_regions[region_id]->padding_x = px + gfx_regions[region_id]->border_width + parent->border_width + parent->padding_x;
}

/**
 * gfx_set_padding_y - set top/bottom padding for a region.
 * @region_id: region component to modify.
 * @py: new vertical padding, excluding border.
 *
 * Description:
 * Adds the component's existing border width to the requested padding,
 * so callers only need to think in terms of padding beyond the border.
 *
 * Return: void.
 */
void gfx_set_padding_y(uint32_t region_id, uint32_t py) {
    if (region_id == PRIMARY_VIEWPORT_ID) {
        gfx_regions[region_id]->padding_y = py + gfx_regions[region_id]->border_width;
        return;
    }
    gfx_region_t *parent              = gfx_regions[PRIMARY_VIEWPORT_ID];
    gfx_regions[region_id]->padding_y = py + gfx_regions[region_id]->border_width + parent->border_width + parent->padding_y;
}

/**
 * gfx_set_bg_color - change a region's background color.
 * @region_id: region component to modify.
 * @color: new background color.
 *
 * Description:
 * Directly sets the bg_color field on the given region component.
 *
 * Return: void.
 */
void gfx_set_bg_color(uint32_t region_id, uint32_t color) {
    gfx_regions[region_id]->bg_color = color;
}

/**
 * gfx_set_fg_color - change a region's foreground/text color.
 * @region_id: region component to modify.
 * @color: new foreground color.
 *
 * Description:
 * Directly sets the fg_color field on the given region component.
 *
 * Return: void.
 */
void gfx_set_fg_color(uint32_t region_id, uint32_t color) {
    gfx_regions[region_id]->fg_color = color;
}

/**
 * gfx_set_border_width - change a region's border width.
 * @region_id: region component to modify.
 * @width: new border width.
 *
 * Description:
 * Directly sets the border_width field on the given region component.
 * Does not recompute existing padding, which is derived from border
 * width only at the time gfx_set_padding_x/gfx_set_padding_y are called.
 *
 * Return: void.
 */
void gfx_set_border_width(uint32_t region_id, uint32_t width) {
    gfx_regions[region_id]->border_width = width;
}

/**
 * gfx_set_border_color - change a region's border color.
 * @region_id: region component to modify.
 * @color: new border color.
 *
 * Description:
 * Directly sets the border_color field on the given region component.
 * Does not repaint the border; call clamp_borders_to_window
 * or gfx_clear_region to make the change visible.
 *
 * Return: void.
 */
void gfx_set_border_color(uint32_t region_id, uint32_t color) {
    gfx_regions[region_id]->border_color = color;
}
