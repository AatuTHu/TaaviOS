#include "kmalloc.h"
#include "config.h"
#include "klog.h"
#include "paging.h"
#include "vmm.h"
#include <stdint.h>

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
    DEBUG_KMALLOC("[KMALLOC][REMAINING_HEAP_SIZE]: Remaining heap size: %d\n", remaining_heap_size);
}

void kmalloc_init(void *heap_start, uint32_t heap_size) {
    DEBUG_KMALLOC("[KMALLOC]: Initializing kmalloc with heap_start: 0x%x\n", heap_start);
    DEBUG_KMALLOC("[KMALLOC]: Heap_size: %d\n", heap_size);
    free_list            = (block_header_t *)heap_start;
    free_list->size      = (heap_size - sizeof(block_header_t));
    free_list->magic     = HEAP_MAGIC;
    free_list->next      = NULL;
    current_heap_ceiling = (uint32_t)heap_start + heap_size;
    DEBUG_KMALLOC("[KMALLOC]: current heap ceiling: 0x%x\n", current_heap_ceiling);
    update_remaining_heap_size();
}

void *kmalloc(uint32_t size) {

    // DEBUG_KMALLOC("[KMALLOC][ALLOC]: Allocating %d bytes\n", size);
    block_header_t *current = free_list;
    block_header_t *prev    = NULL;

    while (1) {
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
                    //   DEBUG_KMALLOC("[KMALLOC][ALLOC]: Block split — new block at 0x%x size: %d\n", split_block, split_block->size);
                }
                if (prev == NULL) {
                    free_list = current->next;
                } else {
                    prev->next = current->next;
                }
                update_remaining_heap_size();
                // DEBUG_KMALLOC("[KMALLOC][ALLOC]: Allocated at 0x%x\n", (uint8_t *)current + sizeof(block_header_t));

                return (void *)((uint8_t *)current + sizeof(block_header_t));
            }
            prev    = current;
            current = current->next;
        }
        // DEBUG_KMALLOC("\n[KMALLOC][ALLOC]: Size asked was bigger than remaining size. Drying to allocate more heap\n");
        int addition_size = (HEAP_PAGES * PAGE_SIZE) * 2;
        if ((addition_size + current_heap_ceiling) >= HEAP_CEIL) {
            ERROR("[KMALLOC][ALLOC]: Heap ceiling achieved. No more memory left to allocate.\n");
            break;
        }

        if (vmm_alloc(&kernel_page_dir, current_heap_ceiling, addition_size, PAGE_PRESENT | PAGE_RW) == STATUS_ERROR) {
            ERROR("[KMALLOC][ALLOC]: Failed to allocate more virtual memory\n");
            break;
        }

        // DEBUG_KMALLOC("[KMALLOC][ALLOC]: Allocation was successful\n");
        block_header_t *new_block = (block_header_t *)current_heap_ceiling;
        new_block->size           = addition_size - sizeof(block_header_t);
        new_block->magic          = HEAP_MAGIC;
        new_block->next           = NULL;
        kfree((void *)((uint8_t *)new_block + sizeof(block_header_t)));

        current_heap_ceiling += addition_size;
        current = free_list;
        prev    = NULL;

        // DEBUG_KMALLOC("[KMALLOC][ALLOC]: heap size: %d\n", free_list->size);
        // DEBUG_KMALLOC("[KMALLOC][ALLOC]: current heap ceiling: 0x%x\n\n", current_heap_ceiling);
        update_remaining_heap_size();
    }

    ERROR("[KMALLOC][ALLOC]: Out of heap memory!\n");

    return NULL;
}

static void merge() {
    block_header_t *current = free_list;
    while (current != NULL && current->next != NULL) {
        // Sanity check to avoid reading garbage/unmapped memory
        if (current->next->magic != HEAP_MAGIC) {
            ERROR("[KMALLOC][MERGE]: Corrupted block magic in free list at 0x%x\n", current->next);
            break;
        }

        if ((block_header_t *)((uint8_t *)current + sizeof(block_header_t) + current->size) == current->next) {
            //  DEBUG_KMALLOC("[KMALLOC][MERGE]: Merging blocks at 0x%x and 0x%x\n", current, current->next);
            current->size += sizeof(block_header_t) + current->next->size;
            // DEBUG_KMALLOC("[KMALLOC][MERGE]: Currents size after merge: %d\n", current->size);

            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
    update_remaining_heap_size();
}

void kfree(void *ptr) {
    if (!ptr)
        return;

    // DEBUG_KMALLOC("[KMALLOC][FREE]: Freeing at 0x%x\n", ptr);
    block_header_t *addr =
        (block_header_t *)((uint8_t *)ptr - sizeof(block_header_t));
    if (addr->magic != HEAP_MAGIC) {
        ERROR("[KMALLOC][FREE]: Invalid magic at 0x%x — expected 0x%x\n", addr,
              HEAP_MAGIC);
        return;
    } else {
        block_header_t *prev    = NULL;
        block_header_t *current = free_list;

        while (current != NULL && current < addr) {
            if (current == addr) {
                ERROR("[KMALLOC][FREE]: Double free detected at 0x%x\n", ptr);
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
        // DEBUG_KMALLOC("[KMALLOC][FREE]: Block returned to free list, merging\n");
        merge();
    }
}
