#ifndef KMALLOC_H
#define KMALLOC_H
#include <stddef.h>
#include <stdint.h>

typedef struct block_header {
    uint32_t magic;
    uint32_t size;
    struct block_header *next;
} block_header_t;

void kmalloc_init(void *heap_start, uint32_t heap_size);
void *kmalloc(uint32_t size);
void kfree(void *ptr);

#endif