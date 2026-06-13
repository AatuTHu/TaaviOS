#include "paging.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include "mm.h"
#include "pmm.h"

page_directory_t kernel_page_dir __attribute__((aligned(PAGE_SIZE_BYTES)));

void paging_map(page_directory_t *dir, uint32_t virt, uint32_t phys,
    uint32_t flags) {
    if (!dir) {
        ERROR("[PAGING] No directory given for page mapping\n");
        return;
    }
    uint32_t pd_index = virt >> PD_INDEX_SHIFT;
    DEBUG_PAGING("[PAGING] Mapping to page directory index: %d\n", pd_index);
    uint32_t pt_index = (virt >> PAGE_SHIFT) & PT_INDEX_MASK;
    DEBUG_PAGING("[PAGING] Page Table index: %d\n", pt_index);

    uint32_t pd_flags = PAGE_PRESENT | PAGE_RW;
    if (flags & PAGE_USER) {
        pd_flags |= PAGE_USER;
    }

    if (!((*dir)[pd_index] & PAGE_PRESENT)) {
        uint32_t phys_addr = pmm_alloc();
        uint32_t vaddr     = phys_to_virt(phys_addr);
        memset((void *)vaddr, 0, PAGE_SIZE);

        (*dir)[pd_index] = phys_addr | pd_flags;
    }

    uint32_t pt_phys     = (*dir)[pd_index] & ~PAGE_FLAGS_MASK;
    uint32_t *page_table = (uint32_t *)(phys_to_virt(pt_phys));
    page_table[pt_index] = phys | flags | PAGE_PRESENT;
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
    DEBUG("[PAGING][pcd]: Creating virtual page directory\n");
    page_directory_t *phys_addr = (page_directory_t *)
        pmm_alloc(); // ask pmm to allocate a physical page dir
    uint32_t vaddr =
        phys_to_virt((uint32_t)phys_addr); // elevate it to virtual addr
    memset((void *)vaddr, 0, PAGE_SIZE);

    page_directory_t *virt_dir = (page_directory_t *)vaddr;

    memcpy(&(*virt_dir)[KERNEL_PD_INDEX_START],
        &kernel_page_dir[KERNEL_PD_INDEX_START],
        KERNEL_PD_ENTRIES * sizeof(uint32_t));
    DEBUG("[PAGING][pcd]: Page created\n");
    return (page_directory_t *)(vaddr);
}

void paging_switch(page_directory_t *dir) {
    switch_page_dir((uint32_t)dir);
}

void paging_init() {
    DEBUG("[PAGING] Starting to initialize PAGING\n");
    for (int i = 0; i < ENTRIES_PER_TABLE; i++) { kernel_page_dir[i] = 0; }
    DEBUG("[PAGING] PAGE DIRECTORY INITIALIZED\n");

    for (uint32_t i = 0; i < ENTRIES_PER_TABLE; i++) {
        paging_map(&kernel_page_dir, KERNEL_VIRTUAL_START + i * PAGE_SIZE,
            KERNEL_PHYSICAL_ADDRESS + i * PAGE_SIZE,
            PAGE_PRESENT | PAGE_RW);
    }

    paging_map(&kernel_page_dir, VGA_MEMORY_ADDRESS, VGA_PHYSICAL_ADDRESS,
        PAGE_PRESENT | PAGE_RW);
    DEBUG("[PAGING] VGA page created\n");
    DEBUG("[PAGING] SWITCHING TO KERNEL PAGE DIRECTORY\n");
    paging_switch(&kernel_page_dir);
    DEBUG("[PAGING] PAGING INITIALIZED SUCCESFULLY\n");
}

uint32_t paging_get_phys(page_directory_t *dir, uint32_t virt) {
    if (!dir) {
        ERROR("[PAGING] No directory given, cant give physical page\n");
        return INVALID_PHYSICAL_PAGE; // Kinda not the right word. But if
                                      // directory missing we cant give a valid
                                      // page? I think this is right.
    }
    DEBUG("[PAGING]: getting physical location for page directory: %x, with "
          "virt: %d\n",
        (uint32_t)dir, virt);
    uint32_t pd_index = virt >> PD_INDEX_SHIFT;
    DEBUG("[PAGING]: page directory index: %d\n", pd_index);
    if (!((*dir)[pd_index] & PAGE_PRESENT)) {
        DEBUG("[PAGING] page not found\n");
        return INVALID_PHYSICAL_PAGE;
    }

    uint32_t pt_index = (virt >> PAGE_SHIFT) & PT_INDEX_MASK;
    DEBUG("[PAGING]: page table index: %d\n", pt_index);
    uint32_t pt_virt = phys_to_virt(((*dir)[pd_index] & PAGE_FRAME_MASK));
    DEBUG("[PAGING]: virtual page table: %x\n", pt_virt);
    page_table_t *pt = (page_table_t *)pt_virt;

    return (*pt)[pt_index] & PAGE_FRAME_MASK;
}