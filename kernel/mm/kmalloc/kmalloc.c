#include "kmalloc.h"
#include "config.h"
#include "klog.h"

static block_header_t* free_list = NULL;

void kmalloc_init(void* heap_start, uint32_t heap_size) {
    DEBUG("[KMALLOC] Initialzing kmalloc with heap_start: %x\n", heap_start);
    DEBUG("[KMALLOC] Heap_size: %d\n", heap_size);
    
    free_list = (block_header_t*)heap_start;
    
    free_list->size = (heap_size - sizeof(block_header_t));
    free_list->magic = HEAP_MAGIC;
    free_list->next = NULL;
}

void* kmalloc(uint32_t size) {
    block_header_t *current = free_list;
    block_header_t *prev = NULL;

    while(current != NULL) {
        if(current->size >= size) {
            
            if(current->size >= size + sizeof(block_header_t) + 1) {
                block_header_t *split_block = (block_header_t*)((uint8_t*)current + sizeof(block_header_t) + size);
                
                split_block->size = current->size - size - sizeof(block_header_t);
                split_block->magic = HEAP_MAGIC;
                split_block->next = current->next;
                
                current->size = size;
                current->next = split_block;
            }

            if (prev == NULL) {
                free_list = current->next;
            } else {
                prev->next = current->next;
            }

            return (void*)((uint8_t*)current + sizeof(block_header_t));
        }
        prev = current;
        current = current->next;
    }
    
    return NULL;
}

static void merge() {
    block_header_t *current = free_list;
    while (current != NULL && current->next != NULL)
    {
        if((block_header_t*)((uint8_t*)current + sizeof(block_header_t) + current->size) == current->next) {
            current->size += sizeof(block_header_t) + current->next->size;
            current->next = current->next->next;
        } else {
            current = current->next;
        }
    }
}

void kfree(void* ptr) {
    block_header_t *addr = (block_header_t*)((uint8_t*)ptr - sizeof(block_header_t));
    
    if(addr->magic != HEAP_MAGIC) {
        return;
    } else {
        block_header_t *prev = NULL;
        block_header_t *current = free_list;
        
        while(current != NULL && current < addr) {
            prev = current;
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