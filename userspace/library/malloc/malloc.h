
#ifndef MALLOC_H
#define MALLOC_H
#include <stddef.h>
#include <stdint.h>

#define HEAP_MAGIC 0xDEADBEEF

typedef struct block_header {
    uint32_t magic;
    uint32_t size;
    struct block_header *next;
} block_header_t;

void malloc_init(void *heap_start, uint32_t heap_size);
void *malloc(uint32_t size);
void free(void *ptr);

#endif
