#include "pmm.h"
#include "boot_info.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include <stdint.h>

#define MiB 1048576
static uint64_t *bitmap    = NULL;
static uint64_t max_pages  = 0;
static uint32_t used_pages = 0;
static uint32_t free_pages = 0;
static uint32_t last_found = 0;

static void __pmm_set_bit(const uint64_t page) {
    uint32_t ind = page / 64;
    bitmap[ind]  = bitmap[ind] | (1ULL << (page % 64));
}

static void __pmm_clear_bit(const uint64_t page) {
    uint32_t ind = page / 64;
    bitmap[ind]  = bitmap[ind] & ~(1ULL << (page % 64));
}

static int __pmm_test_bit(const uint64_t page) {
    uint32_t ind = page / 64;
    return (bitmap[ind] & (1ULL << (page % 64))) != 0;
}

uint64_t pmm_get_free_pages(void) {
    return free_pages;
}

uint64_t pmm_get_used_pages(void) {
    return used_pages;
}

uint64_t pmm_get_total_pages(void) {
    return max_pages;
}

/**
 * pmm_reclaim_bootloader - Adds reclaimable regions to the memory pool.
 * @bi: Holds Limine bootloader information.
 *
 * Description:
 * This function adds bootloader-reclaimable memory regions to the physical
 * memory manager's available pool. It loops over all memory regions, targets
 * non-empty reclaimable regions, aligns them to 4 KiB page boundaries, and frees
 * them page by page.
 *
 * Any addresses below 1 MiB are explicitly skipped to protect legacy BIOS
 * structures, the Extended BIOS Data Area (EBDA), and critical ACPI tables
 * required by the kernel.
 *
 * Return: VOID.
 */
void pmm_reclaim_bootloader(const struct boot_info *bi) {
    uint64_t success_count = 0;

    for (uint64_t i = 0; i < bi->region_count; i++) {
        struct boot_mem_region curr_region = bi->regions[i];

        if (curr_region.type == BOOT_REGION_RECLAIMABLE && curr_region.length != 0) {
            uint64_t base = curr_region.base;
            uint64_t top  = curr_region.base + curr_region.length;

            base          = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            top           = top & ~(PAGE_SIZE - 1);

            if (base < top) {
                for (uint64_t addr = base; addr < top; addr += PAGE_SIZE) {

                    if (addr < MiB) {
                        continue;
                    }

                    if (pmm_free(addr) == STATUS_OK) {
                        success_count++;
                    }
                }
            }
        }
    }

    DEBUG_CORE_MM("[PMM][RECLMEM]: Reclaimed %d pages\n", success_count);
}

/**
 * pmm_init - used to initialize physical memory manager for the available memory.
 * @boot_info *bi: holds memory information gathered via limine request.
 *
 * Description:
 * This function creates a basic bitmap physical manager out of the usable regions limine has
 * provided the kernel. First we look out of the usable regions the one that ends with the highest
 * address so that we don't create bitmap any lengthier than it has to be. If we find a potential top
 * we calculate how many pages it can fit and how many bytes it holds. Then we look for the regions
 * for bottom address that is above one MiB but can too hold the size of the bitmap in it. If found
 * we set the bitmap to point at the top of the line. Then we round all regions to be exactly
 * 4096bits and simultaneously freeing them. After this the function re reserves the pages that the
 * bitmap uses and the very first page so that later when allocating the 0 can be used as OOM. (out
 * of mana :3 ). And that we don't accidentally allocate the bitmap itself.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int pmm_init(const struct boot_info *bi) {
    DEBUG_CORE_MM("[PMM][INIT]: Okay setting up the physical memory manager\n");

    int highest_region_index      = -1;
    uint64_t potential_bitmap_top = 0;
    for (uint64_t i = 0; i < bi->region_count; i++) {
        struct boot_mem_region curr_region = bi->regions[i];
        if ((curr_region.type == BOOT_REGION_USABLE || curr_region.type == BOOT_REGION_RECLAIMABLE) &&
            curr_region.base + (uint64_t)curr_region.length > (uint64_t)potential_bitmap_top) {
            potential_bitmap_top = curr_region.base + (uint64_t)curr_region.length;
            highest_region_index = i;
        }
    }

    if (highest_region_index == -1) {
        ERROR("[PMM][INIT]: There was an error locating top for the bitmap.\n");
        return STATUS_ERROR;
    }

    max_pages              = potential_bitmap_top / PAGE_SIZE;
    uint64_t bitmap_bytes  = (max_pages + 7) / 8;

    int bitmap_base_region = -1;

    for (uint64_t i = 0; i < bi->region_count; i++) {
        struct boot_mem_region curr_region = bi->regions[i];
        if (curr_region.type == BOOT_REGION_USABLE && curr_region.base >= MiB && curr_region.length >= bitmap_bytes) {
            bitmap_base_region = i;
            break;
        }
    }

    if (bitmap_base_region == -1) {
        ERROR("[PMM][INIT]: There was an error locating bottom for the bitmap.\n");
        return STATUS_ERROR;
    }

    uint64_t bitmap_phys_addr = bi->regions[bitmap_base_region].base;

    bitmap                    = (uint64_t *)(bitmap_phys_addr + bi->hhdm_offset);
    memset(bitmap, 0xFF, bitmap_bytes);

    DEBUG_CORE_MM("[PMM][INIT]: Rounding all available regions to page size\n");
    for (uint64_t i = 0; i < bi->region_count; i++) {
        struct boot_mem_region curr_region = bi->regions[i];
        if (curr_region.type == BOOT_REGION_USABLE) {
            uint64_t base = curr_region.base;
            uint64_t top  = curr_region.base + curr_region.length;
            base          = (base + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1);
            top           = top & ~(PAGE_SIZE - 1);
            for (uint64_t addr = base; addr < top; addr += PAGE_SIZE) {
                __pmm_clear_bit(addr / PAGE_SIZE);
                free_pages++;
            }
            DEBUG_CORE_MM("[PMM][INIT]: Region %d rounded\n", i);
        }
    }

    uint64_t bitmap_start_page = bitmap_phys_addr / PAGE_SIZE;
    uint64_t bitmap_page_count = (bitmap_bytes + PAGE_SIZE - 1) / PAGE_SIZE;

    for (uint64_t i = 0; i < bitmap_page_count; i++) {
        __pmm_set_bit(bitmap_start_page + i);
        used_pages++;
        free_pages--;
    }

    if (__pmm_test_bit(0) == 0) {
        __pmm_set_bit(0);
        used_pages++;
        free_pages--;
    }

    used_pages = max_pages - free_pages;

    DEBUG_CORE_MM("[PMM][INIT]: Physical memory manager initialized\n");
    DEBUG_CORE_MM("[PMM][INIT]: Max pages: %d, Used pages: %d, free pages: %d\n", max_pages, used_pages, free_pages);
    DEBUG_CORE_MM("[PMM][INIT]: Bitmap physical address 0x%x and size: %d MiB\n", bitmap_phys_addr, (potential_bitmap_top / MiB));

    return STATUS_OK;
}

/**
 * pmm_alloc - used to allocate some sweet physical memory.
 *
 * Description:
 * The function looks for the first page from the bitmap that is free. Then returns the address of it.
 * The function uses pristine optimization tech from the future by marking the last found page up.
 * So that later when somebody dares to ask for a page the loop can be started from the next page
 * after the last one given. If none is found in the first loop we default to start at the first
 * index forward. If not found then it defaults to 0 that means out of memory.
 *
 * Return: 0 on out of memory || address of the page(physical).
 */
uint64_t pmm_alloc(void) {
    //  DEBUG_CORE_MM("[PMM][ALLOC]: Allocating a page of physical memory\n");
    uint64_t page_index = last_found;

    while (page_index < max_pages) {
        if (__pmm_test_bit(page_index) == 0) {
            __pmm_set_bit(page_index);
            used_pages++;
            free_pages--;
            last_found = page_index + 1;
            //   DEBUG_CORE_MM("[PMM][ALLOC]: Found a page on the first loop. pg_ind: 0x%x\n", page_index * PAGE_SIZE);
            //     DEBUG_CORE_MM("[PMM][ALLOC]: Last_found now 0x%x\n", last_found);
            return (page_index * PAGE_SIZE);
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
            //   DEBUG_CORE_MM("[PMM][ALLOC]: Found a page on the second loop. pg_ind: 0x%x\n", page_index * PAGE_SIZE);
            //   DEBUG_CORE_MM("[PMM][ALLOC]: Last_found now 0x%x\n", last_found);
            return (page_index * PAGE_SIZE);
        }
        page_index++;
    }

    DEBUG_CORE_MM("[PMM][ALLOC]: Out of memory!\n");
    return 0;
}

/**
 * PMM_FREE - used to free physical memory.
 * @uint64_t phys: page to be freed.
 *
 * Description:
 * First this function checks if the given page is even possible to be freed. Then if it has been
 * freed and finally frees it.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int pmm_free(const uint64_t phys) {
    if (phys % PAGE_SIZE != 0) {
        ERROR("[PMM][FREE]: Unaligned physical address 0x%x\n", phys);
        return STATUS_ERROR;
    }

    uint64_t page_index = phys / PAGE_SIZE;
    if (page_index >= max_pages) {
        ERROR("[PMM][FREE]: Invalid page index 0x%x\n", page_index);
        return STATUS_ERROR;
    }

    // DEBUG_CORE_MM("[PMM][FREE]: Freeing index %d\n", page_index);

    if (__pmm_test_bit(page_index) == 0) {
        ERROR("[PMM][FREE]: Page already free\n");
        return STATUS_OK;
    }

    __pmm_clear_bit(page_index);
    used_pages--;
    free_pages++;

    if (page_index < last_found) {
        DEBUG_CORE_MM("[PMM][FREE]: Moving last found index to 0x%x\n", page_index);
        last_found = page_index;
    }

    // DEBUG_CORE_MM("[PMM][FREE]: Page freed\n");
    return STATUS_OK;
}
