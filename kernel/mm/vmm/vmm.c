#include "vmm.h"
#include "config.h"
#include "klog.h"
#include "pmm.h"

int vmm_alloc(page_directory_t *dir, uint32_t virt, uint32_t size, uint32_t flags) {
    if (!dir) {
        DEBUG("DIRECTORY NOT GIVEN ABORTING ALLOCATION\n");
        return -1;
    }
    
    DEBUG("ALLOCATING VIRTUAL MEMORY\n");
    DEBUG("Virtual address: %x\n", virt);
    DEBUG("Size: %d\n", size);
    DEBUG("Flags: %d\n", flags);

    uint32_t n_pages = size / PAGE_SIZE;
    DEBUG("Number of pages to be allocated: %d\n", n_pages);
    uint32_t virt_start = virt;
    for(int i = 0; i < n_pages; i++) {
        uint32_t addr = pmm_alloc();
        if(addr == 0) {
            DEBUG("NO PHYSICAL MEMORY LEFT FOR THE ALLOCATION\n");
            DEBUG("STARTING ROLLBACK\n");
            uint32_t rollback = virt_start;
            for(int j = i; j >= 0; j--) {
                paging_unmap(dir, virt);
                rollback -= PAGE_SIZE;
            }
            DEBUG("ROLLBACK SUCCESSFUL\n");
            return -1;
        }
        paging_map(dir, virt, addr, flags);
        virt += PAGE_SIZE;
    }
    DEBUG("ALLOCATION SUCCESSFULL\n");
    return 0;
}

void vmm_free(page_directory_t *dir, uint32_t virt, uint32_t size) {
    if (!dir) {
        DEBUG("DIRECTORY NOT GIVEN ABORTING\n");
        return;
    }

    uint32_t n_pages = size / PAGE_SIZE;
    DEBUG("Number of pages to be freed: %d\n", n_pages);
    for(int i = 0; i < n_pages; i++) {
        uint32_t phys_addr = paging_get_phys(dir, virt);
        pmm_free(phys_addr);
        paging_unmap(dir, virt);
        virt += PAGE_SIZE;
    }
}

void vmm_switch(page_directory_t *dir) {
    paging_switch(dir);
}

page_directory_t *vmm_create_directory(void) {
    return paging_create_directory();
}