#include "pmm.h"
#include "config.h"
#include "klog.h"
#include "mm.h"
#include <kstring.h>

static uint32_t bitmap[MAX_PAGES / 32];
static uint32_t used_pages = 0;
static uint32_t free_pages = 0;
static uint32_t last_found = 0;

/**
 * __pmm_set_bit - Sets a bit on a slot as used.
 * @param page: the 32bit slot where the bit recides
 *
 * Description:
 * By dividing the page with 32 we get the index on the bitmap. For example
 * 34/32 = 1 on bitmap, 25/32 = 0
 * After that by modulo operating the page with %32 we get the right bit that we want to put on.
 *
 */
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

/**
 * pmm_int - Initialize physical memory manager.
 * @param mboot: multiboot info structure that hold info of the system memory
 *
 * Description:
 * Calculates total memory in kb and total page count. After that it
 * converts mmap_addr in to virtual address (vaddr) and calcualtes virtual address end by
 * adding to vaddr the mmap_length. Then for all vaddr entries it casts them as mmap_entry
 * and marks available slots of the bitmap as free to use. Leaving reserved spaces such as bios as used.
 *
 * And finally it reserves slots for kernel
 *
 */
void pmm_init(const struct multiboot_info *mboot) {
    DEBUG_CORE_MM("[PMM] mem_upper: 0x%x\n", mboot->mem_upper);
    uint32_t total_memory_kb = mboot->mem_upper + CONVENTIONAL_MEMORY_KB;
    uint32_t total_pages     = (total_memory_kb * 1024) / PAGE_SIZE;
    DEBUG_CORE_MM("[PMM] Total memory: %d kb\n", total_memory_kb);
    DEBUG_CORE_MM("[PMM] Total pages: %d\n", total_pages);

    // 0xFF same as used
    uint32_t bitmap_size = (total_pages + 7) / 8;
    memset(bitmap, 0xFF, bitmap_size);

    uint32_t vaddr = phys_to_virt(mboot->mmap_addr);
    uint32_t vend  = vaddr + mboot->mmap_length;

    DEBUG_CORE_MM("[PMM] virtual address start: 0x%x\n", vaddr);
    DEBUG_CORE_MM("[PMM] virtual address end: 0x%x\n", vend);

    DEBUG_CORE_MM("[PMM] Memory map entries:\n");
    while (vaddr < vend) {
        const struct mmap_entry *entry = (struct mmap_entry *)vaddr;
        DEBUG_CORE_MM("[PMM] [%s] base=0x%x length=0x%x\n", entry->type == 1 ? "AVAILABLE" : "RESERVED ", entry->base_low, entry->length_low);

        if (entry->type == 1) {
            uint32_t start_page = entry->base_low / PAGE_SIZE;
            uint32_t num_pages  = entry->length_low / PAGE_SIZE;
            for (uint32_t i = 0; i < num_pages; i++) {
                if ((start_page + i) < total_pages) {
                    __pmm_clear_bit(start_page + i);
                }
            }
        }

        vaddr = vaddr + entry->size + 4;
    }

    DEBUG_CORE_MM("[PMM] Reserving low memory (pages 0-%d)\n", RESERVED_LOW_PAGES - 1);
    for (uint32_t i = 0; i < RESERVED_LOW_PAGES; i++) {
        __pmm_set_bit(i);
        last_found = i;
    }

    DEBUG_CORE_MM("[PMM]: Marking last found slot to be: %d\n", last_found);

    extern uint32_t _kernel_start;
    extern uint32_t _kernel_end;
    uint32_t kernel_start_page = (uint32_t)&_kernel_start / PAGE_SIZE;
    uint32_t kernel_end_page   = (uint32_t)&_kernel_end / PAGE_SIZE;
    DEBUG_CORE_MM("[PMM] Kernel pages: %d-%d (0x%x-0x%x)\n", kernel_start_page,
                  kernel_end_page, kernel_start_page * PAGE_SIZE,
                  kernel_end_page * PAGE_SIZE);
    for (uint32_t i = kernel_start_page; i < kernel_end_page; i++) {
        __pmm_set_bit(i);
    }

    uint32_t bitmap_phys       = (uint32_t)bitmap - KERNEL_VIRTUAL_BASE;
    uint32_t bitmap_size_bytes = (total_pages + 31) / 32 * sizeof(uint32_t);
    uint32_t bitmap_start      = bitmap_phys / PAGE_SIZE;
    uint32_t bitmap_end        = (bitmap_phys + bitmap_size_bytes - 1) / PAGE_SIZE;
    DEBUG_CORE_MM("[PMM] Bitmap pages: %d-%d (phys=0x%x size=%d bytes)\n", bitmap_start,
                  bitmap_end, bitmap_phys, bitmap_size_bytes);

    // Left out for now as bitmap fits inside kernel slots.
    //  for (uint32_t i = bitmap_start; i <= bitmap_end; i++) { __pmm_set_bit(i); }

    free_pages = 0;
    used_pages = 0;
    for (uint32_t i = 0; i < total_pages; i++) {
        if (__pmm_test_bit(i))
            used_pages++;
        else
            free_pages++;
    }

    DEBUG_CORE_MM("[PMM] PMM ready — free: %d pages (%d kb), used: %d pages (%d kb)\n",
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

int pmm_free(uint32_t addr) {
    uint32_t page_index = addr / PAGE_SIZE;
    // DEBUG_CORE_MM("[PMM]: freeing page index: %d\n", page_index);
    if (!__pmm_test_bit(page_index)) {
        ERROR("[PMM]: No page at that index\n");
        return STATUS_ERROR;
    }
    __pmm_clear_bit(page_index);
    used_pages--;
    free_pages++;
    // DEBUG_CORE_MM("[PMM]: page freed\n");
    return STATUS_OK;
}

uint32_t pmm_get_used_pages(void) {
    return used_pages;
}

uint32_t pmm_get_free_pages(void) {
    return free_pages;
}
