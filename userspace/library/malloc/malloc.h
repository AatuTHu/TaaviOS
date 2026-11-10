
#ifndef MALLOC_H
#define MALLOC_H
#include <stddef.h>
#include <stdint.h>

#define HEAP_MAGIC 0xDEADBEEF
#define PAGE_SIZE 4096
#define ADDITIONAL_HEAP_PAGE_SIZE 128

typedef struct block_header {
    uintptr_t magic;
    uintptr_t size;
    struct block_header *next;
} block_header_t;

void malloc_init(void *heap_start, uintptr_t heap_size);
void *malloc(uintptr_t size);
void free(void *ptr);

#endif
