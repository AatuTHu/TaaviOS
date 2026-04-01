#include "pmm.h"
#include "kstring.h"
#include "config.h"

static uint32_t bitmap[MAX_PAGES / 32];
static uint32_t used_pages = 0;
static uint32_t free_pages = 0;
static uint32_t last_found = 0;

/**
 * Marks a page as "In Use" (Set bit to 1).
 */
void pmm_set_bit(uint32_t page) {
    uint32_t ind = page / 32; 
    bitmap[ind] = bitmap[ind] | (1 << (page % 32));
}

/**
 * Marks a page as "Available" (Clear bit to 0).
 */
void pmm_clear_bit(uint32_t page) {
    uint32_t ind = page / 32; 
    bitmap[ind] = bitmap[ind] & ~(1 << (page % 32));
}

/**
 * Checks the status of a specific physical frame.
 */
int pmm_test_bit(uint32_t page) {
    uint32_t ind = page / 32; 
    return bitmap[ind] & (1 << (page % 32));
}

void pmm_init(struct multiboot_info *mboot) {
    uint32_t total_memory_kb = mboot->mem_upper + CONVENTIONAL_MEMORY_KB;
    uint32_t total_pages = (total_memory_kb * 1024) / PAGE_SIZE; // KB -> bytes -> pages
    
    memset(bitmap, 0, (total_pages/8));

    uint32_t addr = mboot->mmap_addr + KERNEL_VIRTUAL_BASE; 
    uint32_t end = addr + mboot->mmap_length;
    
    while (addr < end) {
        struct mmap_entry *entry = (struct mmap_entry *)addr;  
        if (entry->type == 1) {
            uint32_t start_page = entry->base_low / PAGE_SIZE;
            uint32_t num_pages = entry->length_low / PAGE_SIZE;
            
            for (uint32_t i = 0; i < num_pages; i++) {
                pmm_clear_bit(start_page + i);
            }
        }
        addr = addr + entry->size + 4;
    }

    for (uint32_t i = 0; i < RESERVED_LOW_PAGES; i++) {
        pmm_set_bit(i);
    }

    extern uint32_t _kernel_start;
    extern uint32_t _kernel_end;

    uint32_t kernel_start_page = (uint32_t)&_kernel_start / PAGE_SIZE;
    uint32_t kernel_end_page   = (uint32_t)&_kernel_end   / PAGE_SIZE;

    for (uint32_t i = kernel_start_page; i < kernel_end_page; i++) {
        pmm_set_bit(i);
    }

    uint32_t bitmap_phys  = (uint32_t)bitmap - KERNEL_VIRTUAL_BASE;
    uint32_t bitmap_size  = (MAX_PAGES / 32) * sizeof(uint32_t);
    uint32_t bitmap_start = bitmap_phys / PAGE_SIZE;
    uint32_t bitmap_end   = (bitmap_phys + bitmap_size) / PAGE_SIZE;

    for (uint32_t i = bitmap_start; i < bitmap_end; i++) {
        pmm_set_bit(i);
    }

    /* Final tally of our resources */
    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        if (pmm_test_bit(i)) used_pages++;
        else free_pages++;
    }
}

uint32_t pmm_alloc() {
    uint32_t page_index = last_found;
   
    while (page_index < MAX_PAGES) {
        if(pmm_test_bit(page_index) == 0) {
            pmm_set_bit(page_index);
            used_pages++;
            free_pages--;
            last_found = page_index + 1;
            return (page_index * PAGE_SIZE); // Convert index back to physical address
        }
        page_index++;
    }

    page_index = 0;
    while(page_index < last_found) {
        if(pmm_test_bit(page_index) == 0) {
            pmm_set_bit(page_index);
            used_pages++;
            free_pages--;
            last_found = page_index + 1;
            return (page_index * PAGE_SIZE); // Convert index back to physical address
        }
        page_index++;
    }

    return 0; // Out of Physical Memory (Panic territory)
}

void pmm_free(uint32_t addr) {
    uint32_t page_index = addr / PAGE_SIZE;
    if(!pmm_test_bit(page_index)) { return; }
    pmm_clear_bit(page_index);
    used_pages--;
    free_pages++;
}

uint32_t pmm_get_used_pages(void) {
    return used_pages;
}

uint32_t pmm_get_free_pages(void) {
    return free_pages;
}