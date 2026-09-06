#include "malloc.h"
#include "log.h"

/**
 * Heap allocator
 * Design & Implementation:
 * Inspiration from osdev & countless other osdev projects
 * @author: A.H, 2026
 */

static block_header_t *free_list = NULL;
static uint32_t remaining_heap_size;
static uint32_t current_heap_ceiling = 0;

static void update_remaining_heap_size() {
    size_t total_free       = 0;
    block_header_t *current = free_list;
    while (current != NULL) {
        total_free += sizeof(block_header_t) + current->size;
        current = current->next;
    }
    remaining_heap_size = total_free;
}

void malloc_init(void *heap_start, uint32_t heap_size) {
    free_list            = (block_header_t *)heap_start;
    free_list->size      = (heap_size - sizeof(block_header_t));
    free_list->magic     = HEAP_MAGIC;
    free_list->next      = NULL;
    current_heap_ceiling = (uint32_t)heap_start + heap_size;
    update_remaining_heap_size();
}

void *malloc(uint32_t size) {
    block_header_t *current = free_list;
    block_header_t *prev    = NULL;
    while (current != NULL) {
        if (current->size >= size) {
            if (current->size >= size + sizeof(block_header_t) + 1) {
                block_header_t *split_block =
                    (block_header_t *)((uint8_t *)current +
                                       sizeof(block_header_t) + size);
                split_block->size =
                    current->size - size - sizeof(block_header_t);
                split_block->magic = HEAP_MAGIC;
                split_block->next  = current->next;
                current->size      = size;
                current->next      = split_block;
            }
            if (prev == NULL) {
                free_list = current->next;
            } else {
                prev->next = current->next;
            }
            update_remaining_heap_size();
            return (void *)((uint8_t *)current + sizeof(block_header_t));
        }
        prev    = current;
        current = current->next;
    }

    LOG("No heap memory left\n");
    return NULL;
}
static void merge() {
    block_header_t *current = free_list;
    while (current != NULL && current->next != NULL) {
        if (current->next->magic != HEAP_MAGIC) {
            break;
        }

        if ((block_header_t *)((uint8_t *)current + sizeof(block_header_t) + current->size) == current->next) {
            current->size += sizeof(block_header_t) + current->next->size;

            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
    update_remaining_heap_size();
}

void free(void *ptr) {
    if (!ptr)
        return;

    block_header_t *addr =
        (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    if (addr->magic != HEAP_MAGIC) {
        LOG("Invalid heap magic\n");
        return;
    } else {
        block_header_t *prev    = NULL;
        block_header_t *current = free_list;

        while (current != NULL && current < addr) {
            if (current == addr) {
                return;
            }
            prev    = current;
            current = current->next;
        }

        addr->next = current;
        if (prev == NULL) {
            free_list = addr;
        } else {
            prev->next = addr;
        }
        merge();
    }
}
