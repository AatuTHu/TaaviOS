#include "paging.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include "mm.h"
#include "pmm.h"

#define MAX_DEFERRED_MAPPINGS 2
page_directory_t kernel_page_dir __attribute__((aligned(PAGE_SIZE_BYTES)));
static int mappings_added = 0;
deferred_map_entry_t deferred_mappings[MAX_DEFERRED_MAPPINGS];

int paging_map(page_directory_t *dir, uint32_t virt, uint32_t phys,
               uint32_t flags) {
    if (!dir) {
        ERROR("[PAGING] No directory given for page mapping\n");
        return STATUS_ERROR;
    }
    uint32_t pd_index = virt >> PD_INDEX_SHIFT;
    uint32_t pt_index = (virt >> PAGE_SHIFT) & PT_INDEX_MASK;

    uint32_t pd_flags = PAGE_PRESENT | PAGE_RW;
    if (flags & PAGE_USER) {
        pd_flags |= PAGE_USER;
    }

    if (!((*dir)[pd_index] & PAGE_PRESENT)) {
        uint32_t phys_addr = pmm_alloc();

        if (phys_addr == 0) {
            ERROR("[PAGING][ALLOC]: Out of physical memory\n");
            return STATUS_ERROR;
        }

        uint32_t vaddr = phys_to_virt(phys_addr);
        memset((void *)vaddr, 0, PAGE_SIZE);

        (*dir)[pd_index] = phys_addr | pd_flags;
    }

    uint32_t pt_phys     = (*dir)[pd_index] & ~PAGE_FLAGS_MASK;
    uint32_t *page_table = (uint32_t *)(phys_to_virt(pt_phys));
    page_table[pt_index] = phys | flags | PAGE_PRESENT;
    return STATUS_OK;
}

void paging_unmap(page_directory_t *dir, uint32_t virt) {
    if (!dir)
        return;
    uint32_t pd_index    = virt >> PD_INDEX_SHIFT;
    uint32_t pt_index    = (virt >> PAGE_SHIFT) & PT_INDEX_MASK;
    uint32_t pt_phys     = (*dir)[pd_index] & ~PAGE_FLAGS_MASK;
    uint32_t *page_table = (uint32_t *)(phys_to_virt(pt_phys));
    page_table[pt_index] = 0;

    __asm__ __volatile__("invlpg (%0)" ::"r"(virt) : "memory");
}

page_directory_t *paging_create_directory() {
    DEBUG_CORE_MM("[PAGING][pcd]: Creating virtual page directory\n");
    uint32_t phys_page = pmm_alloc(); // ask pmm to allocate a physical page dir
    if (phys_page == 0) {
        ERROR("[PAGING][pcd]: pmm allocated invalid address\n");
        return NULL;
    }
    page_directory_t *phys_addr = (page_directory_t *)phys_page;
    uint32_t vaddr              = phys_to_virt((uint32_t)phys_addr); // elevate it to virtual addr
    memset((void *)vaddr, 0, PAGE_SIZE);

    page_directory_t *virt_dir = (page_directory_t *)vaddr;

    memcpy(&(*virt_dir)[KERNEL_PD_INDEX_START],
           &kernel_page_dir[KERNEL_PD_INDEX_START],
           KERNEL_PD_ENTRIES * sizeof(uint32_t));
    DEBUG_CORE_MM("[PAGING][pcd]: Page created\n");
    return (page_directory_t *)(vaddr);
}

void paging_switch(page_directory_t *dir) {
    switch_page_dir((uint32_t)dir);
}

int paging_add_deferred_mapping(page_directory_t *dir, uint32_t virt, uint32_t phys, uint32_t flags, uint32_t page_count) {

    if (mappings_added == 0) {
        memset(&deferred_mappings, 0, sizeof(deferred_mappings));
    }

    if (mappings_added == MAX_DEFERRED_MAPPINGS) {
        return STATUS_ERROR;
    }
    deferred_mappings[mappings_added].dir        = dir;
    deferred_mappings[mappings_added].virt       = virt;
    deferred_mappings[mappings_added].phys       = phys;
    deferred_mappings[mappings_added].flags      = flags;
    deferred_mappings[mappings_added].page_count = page_count;
    mappings_added++;
    return STATUS_OK;
}

void paging_init() {
    DEBUG_CORE_MM("[PAGING] Starting to initialize PAGING\n");
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) {
        kernel_page_dir[i] = 0;
    }
    DEBUG_CORE_MM("[PAGING] PAGE DIRECTORY INITIALIZED\n");

    for (uint32_t i = 0; i < ENTRIES_PER_TABLE; i++) {
        paging_map(&kernel_page_dir, KERNEL_VIRTUAL_START + i * PAGE_SIZE,
                   KERNEL_PHYSICAL_ADDRESS + i * PAGE_SIZE,
                   PAGE_PRESENT | PAGE_RW);
    }

    paging_map(&kernel_page_dir, VGA_MEMORY_ADDRESS, VGA_PHYSICAL_ADDRESS,
               PAGE_PRESENT | PAGE_RW);

    DEBUG_FB("[FB][MAP_FB_PAGE]: Mapping framebuffer\n");
    for (int j = 0; j < mappings_added; j++) {
        if (deferred_mappings[j].flags != 0) {
            for (uint32_t i = 0; i < deferred_mappings[j].page_count; i++) {
                paging_map(deferred_mappings[j].dir,
                           deferred_mappings[j].virt + i * PAGE_SIZE,
                           deferred_mappings[j].phys + i * PAGE_SIZE,
                           deferred_mappings[j].flags);
            }
        }
    }
    DEBUG_FB("[FB][MAP_FB_PAGE]: Mapping successfull\n");
    DEBUG_CORE_MM("[PAGING] VGA page created\n");
    DEBUG_CORE_MM("[PAGING] SWITCHING TO KERNEL PAGE DIRECTORY\n");
    paging_switch(&kernel_page_dir);
    DEBUG_CORE_MM("[PAGING] PAGING INITIALIZED SUCCESFULLY\n");
}

uint32_t paging_get_phys(page_directory_t *dir, uint32_t virt) {
    if (!dir) {
        ERROR("[PAGING] No directory given, cant give physical page\n");
        return INVALID_PHYSICAL_PAGE; // Kinda not the right word. But if
                                      // directory missing we cant give a valid
                                      // page? I think this is right.
    }
    DEBUG_CORE_MM("[PAGING]: getting physical location for page directory: %x, with "
                  "virt: %d\n",
                  (uint32_t)dir, virt);
    uint32_t pd_index = virt >> PD_INDEX_SHIFT;
    DEBUG_CORE_MM("[PAGING]: page directory index: %d\n", pd_index);
    if (!((*dir)[pd_index] & PAGE_PRESENT)) {
        DEBUG_CORE_MM("[PAGING] page not found\n");
        return INVALID_PHYSICAL_PAGE;
    }

    uint32_t pt_index = (virt >> PAGE_SHIFT) & PT_INDEX_MASK;
    DEBUG_CORE_MM("[PAGING]: page table index: %d\n", pt_index);
    uint32_t pt_virt = phys_to_virt(((*dir)[pd_index] & PAGE_FRAME_MASK));
    DEBUG_CORE_MM("[PAGING]: virtual page table: %x\n", pt_virt);
    page_table_t *pt = (page_table_t *)pt_virt;

    return (*pt)[pt_index] & PAGE_FRAME_MASK;
}