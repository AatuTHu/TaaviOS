#include "pmm.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include "mm.h"

static uint32_t bitmap[MAX_PAGES / 32];
static uint32_t used_pages = 0;
static uint32_t free_pages = 0;
static uint32_t last_found = 0;

void __pmm_set_bit(uint32_t page) {
    uint32_t ind = page / 32;
    bitmap[ind]  = bitmap[ind] | (1U << (page % 32));
}

static void __pmm_clear_bit(uint32_t page) {
    uint32_t ind = page / 32;
    bitmap[ind]  = bitmap[ind] & ~(1U << (page % 32));
}

static int __pmm_test_bit(uint32_t page) {
    uint32_t ind = page / 32;
    return bitmap[ind] & (1U << (page % 32));
}

void pmm_init(const struct multiboot_info *mboot) {
    uint32_t total_memory_kb = mboot->mem_upper + CONVENTIONAL_MEMORY_KB;
    uint32_t total_pages     = (total_memory_kb * 1024) / PAGE_SIZE;
    klog("[PMM] INITIALIZING PHYSICAL MEMORY TO: %d kb\n", total_memory_kb);
    klog("[PMM] Total pages: %d\n", total_pages);

    memset(bitmap, 0, (total_pages / 8));

    uint32_t vaddr = phys_to_virt(mboot->mmap_addr);
    uint32_t vend  = vaddr + mboot->mmap_length;
    klog("[PMM] Memory map entries:\n");
    while (vaddr < vend) {
        const struct mmap_entry *entry = (struct mmap_entry *)vaddr;
        klog("[PMM] [%s] base=0x%x length=0x%x\n",
             entry->type == 1 ? "AVAILABLE" : "RESERVED ", entry->base_low,
             entry->length_low);
        if (entry->type == 1) {
            uint32_t start_page = entry->base_low / PAGE_SIZE;
            uint32_t num_pages  = entry->length_low / PAGE_SIZE;
            for (uint32_t i = 0; i < num_pages; i++) {
                __pmm_clear_bit(start_page + i);
            }
        }
        vaddr = vaddr + entry->size + 4;
    }

    klog("[PMM] Reserving low memory (pages 0-%d)\n", RESERVED_LOW_PAGES - 1);
    for (uint32_t i = 0; i < RESERVED_LOW_PAGES; i++) { __pmm_set_bit(i); }

    extern uint32_t _kernel_start;
    extern uint32_t _kernel_end;
    uint32_t kernel_start_page = (uint32_t)&_kernel_start / PAGE_SIZE;
    uint32_t kernel_end_page   = (uint32_t)&_kernel_end / PAGE_SIZE;
    klog("[PMM] Kernel pages: %d-%d (0x%x-0x%x)\n", kernel_start_page,
         kernel_end_page, kernel_start_page * PAGE_SIZE,
         kernel_end_page * PAGE_SIZE);
    for (uint32_t i = kernel_start_page; i < kernel_end_page; i++) {
        __pmm_set_bit(i);
    }

    uint32_t bitmap_phys  = (uint32_t)bitmap - KERNEL_VIRTUAL_BASE;
    uint32_t bitmap_size  = (MAX_PAGES / 32) * sizeof(uint32_t);
    uint32_t bitmap_start = bitmap_phys / PAGE_SIZE;
    uint32_t bitmap_end   = (bitmap_phys + bitmap_size) / PAGE_SIZE;
    klog("[PMM] Bitmap pages: %d-%d (phys=0x%x size=%d bytes)\n", bitmap_start,
         bitmap_end, bitmap_phys, bitmap_size);
    for (uint32_t i = bitmap_start; i < bitmap_end; i++) { __pmm_set_bit(i); }

    for (uint32_t i = 0; i < MAX_PAGES; i++) {
        if (__pmm_test_bit(i))
            used_pages++;
        else
            free_pages++;
    }
    klog("[PMM] PMM ready — free: %d pages (%d kb), used: %d pages (%d kb)\n",
         free_pages, (free_pages * PAGE_SIZE) / 1024, used_pages,
         (used_pages * PAGE_SIZE) / 1024);
}

uint32_t pmm_alloc() {
    uint32_t page_index = last_found;

    while (page_index < MAX_PAGES) {
        if (__pmm_test_bit(page_index) == 0) {
            __pmm_set_bit(page_index);
            used_pages++;
            free_pages--;
            last_found = page_index + 1;
            return (page_index *
                    PAGE_SIZE); // Convert index back to physical address
        }
        page_index++;
    }

    page_index = 0;
    while (page_index < last_found) {
        if (__pmm_test_bit(page_index) == 0) {
            __pmm_set_bit(page_index);
            used_pages++;
            free_pages--;
            last_found = page_index + 1;
            return (page_index *
                    PAGE_SIZE); // Convert index back to physical address
        }
        page_index++;
    }

    return 0; // Out of Physical Memory (Panic territory)
}

void pmm_free(uint32_t addr) {
    uint32_t page_index = addr / PAGE_SIZE;
    DEBUG("[PMM]: freeing page index: %d\n", page_index);
    if (!__pmm_test_bit(page_index)) {
        DEBUG("[PMM]: No page at that index\n");
        return;
    }
    __pmm_clear_bit(page_index);
    used_pages--;
    free_pages++;
    DEBUG("[PMM]: page freed\n");
}

uint32_t pmm_get_used_pages(void) {
    return used_pages;
}

uint32_t pmm_get_free_pages(void) {
    return free_pages;
}