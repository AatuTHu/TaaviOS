#ifndef READLINE_H
#define READLINE_H

#include <stdint.h>
void readline(char *buf, uint32_t buffer_size);
void readline_at_region(uint32_t region_id, char *buf, uint32_t buffer_size);
void readline_at(uint32_t region_id, char *buf, uint32_t buffer_size, uint32_t x, uint32_t y);
#endif
