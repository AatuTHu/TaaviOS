#include "paging.h"
#include "config.h"
#include "pmm.h"
#include "kstring.h"
#include "mm.h"
#include "klog.h"

page_directory_t kernel_page_dir __attribute__((aligned(4096)));

void paging_map(page_directory_t *dir, uint32_t virt, uint32_t phys, uint32_t flags) {
    if (!dir) {
        ERROR("[PAGING] No directory given for page mapping\n");
        return;
    } 
    uint32_t pd_index = virt >> 22;
    //DEBUG("[PAGING] Mapping to page directory index: %d\n", pd_index);
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    //DEBUG("[PAGING] Page Table index: %d\n", pt_index);

    if (!((*dir)[pd_index] & PAGE_PRESENT)) {
        uint32_t page_addr = pmm_alloc();
        uint32_t page_virt = phys_to_virt(page_addr);
        memset((void*)page_virt, 0, PAGE_SIZE);
        (*dir)[pd_index] = page_addr | PAGE_PRESENT | PAGE_RW;
    }

    uint32_t pt_phys = (*dir)[pd_index] & ~0xFFF;
    uint32_t *page_table = (uint32_t *)(phys_to_virt(pt_phys));
    page_table[pt_index] = phys | flags;
}

void paging_unmap(page_directory_t *dir, uint32_t virt) {
    if (!dir) return;
    uint32_t pd_index = virt >> 22;
    uint32_t pt_index = (virt >> 12) & 0x3FF;
    uint32_t pt_phys = (*dir)[pd_index] & ~0xFFF;
    uint32_t *page_table = (uint32_t *)(phys_to_virt(pt_phys));
    page_table[pt_index] = 0;

    __asm__ __volatile__("invlpg (%0)" :: "r"(virt) : "memory");
}

page_directory_t *paging_create_directory() {
    page_directory_t *page_dir = (page_directory_t*)pmm_alloc(); //ask pmm to allocate a physical page dir
    uint32_t page_virt = phys_to_virt((uint32_t)page_dir); //elevate it to virtual addr
    memset((void*)page_virt, 0, PAGE_SIZE);

    page_directory_t *virt_dir = (page_directory_t*)page_virt;
   
    memcpy(&(*virt_dir)[768], &kernel_page_dir[768], 256 * sizeof(uint32_t));

    return (page_directory_t*)(phys_to_virt((uint32_t)page_dir));
}

void paging_switch(page_directory_t *dir) {
    uint32_t phys = (uint32_t)dir - KERNEL_VIRTUAL_BASE;
    __asm__ __volatile__("mov %0, %%cr3" :: "r"(phys));
}

void paging_init() {
    DEBUG("[PAGING] Starting to initialize PAGING\n");
    for(int i = 0; i < 1024; i++) {
        kernel_page_dir[i] = 0;
    }
    DEBUG("[PAGING] PAGE DIRECTORY INITIALIZED\n");

    for (uint32_t i = 0; i < 1024; i++) {
    paging_map(&kernel_page_dir,
               KERNEL_VIRTUAL_START + i * PAGE_SIZE,
               KERNEL_PHYSICAL_ADDRESS + i * PAGE_SIZE,
               PAGE_PRESENT | PAGE_RW);
    }

    paging_map(&kernel_page_dir, VGA_MEMORY_ADDRESS, VGA_PHYSICAL_ADDRESS, PAGE_PRESENT | PAGE_RW);
    DEBUG("[PAGING] VGA page created\n");
    DEBUG("[PAGING] SWITCHING TO KERNEL PAGE DIRECTORY\n");
    paging_switch(&kernel_page_dir);
    DEBUG("[PAGING] PAGING INITIALIZED SUCCESFULLY\n");
}

uint32_t paging_get_phys(page_directory_t *dir, uint32_t virt) {
    if (!dir) {
        ERROR("[PAGING] No directory given, cant give physical page\n");
        return 0;
    } 
    uint32_t pd_index = virt >> 22;
    if (!((*dir)[pd_index] & PAGE_PRESENT)) return 0; //Page not found


    uint32_t pt_index = (virt >> 12) & 0x3FF;
    uint32_t pt_virt = phys_to_virt(((*dir)[pd_index] & 0xFFFFF000));

    page_table_t *pt = (page_table_t *)pt_virt;

    return (* pt)[pt_index] & 0xFFFFF000;
}