#ifndef KMALLOC_H
#define KMALLOC_H
#include <stddef.h>
#include <stdint.h>

typedef struct block_header {
    uintptr_t magic;
    uintptr_t size;
    struct block_header *next;
} block_header_t;

int kmalloc_init(void *heap_start, uintptr_t heap_size);
void *kmalloc(uintptr_t size);
int kfree(void *ptr);

#endif
