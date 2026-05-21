#include "vmm.h"
#include "pmm.h"
#include "mm.h"
#include "config.h"
#include "klog.h"

int vmm_alloc(page_directory_t *dir, uint32_t virt, uint32_t size, uint32_t flags) {
    if (!dir) {
        ERROR("[VMM]: DIRECTORY NOT GIVEN ABORTING ALLOCATION\n");
        return -1;
    }
    
    DEBUG("[VMM]: ALLOCATING VIRTUAL MEMORY\n");
    DEBUG("[VMM]: Virtual address: %x\n", virt);
    DEBUG("[VMM]: Size: %d\n", size);
    DEBUG("[VMM]: Flags: %d\n", flags);

    uint32_t n_pages = size / PAGE_SIZE;
    DEBUG("[VMM]: Number of pages to be allocated: %d\n", n_pages);
    uint32_t virt_start = virt;
    for(uint32_t i = 0; i < n_pages; i++) {
        uint32_t addr = pmm_alloc();
        if(addr == 0) {
            DEBUG("[VMM]: NO PHYSICAL MEMORY LEFT FOR THE ALLOCATION\n");
            DEBUG("[VMM]: STARTING ROLLBACK\n");
            uint32_t rollback = virt_start;
            for(int j = i; j >= 0; j--) {
                paging_unmap(dir, virt);
                rollback -= PAGE_SIZE;
            }
            DEBUG("[VMM]: ROLLBACK SUCCESSFUL\n");
            return -1;
        }
        paging_map(dir, virt, addr, flags);
        virt += PAGE_SIZE;
    }
    DEBUG("[VMM]: ALLOCATION SUCCESSFULL\n");
    return 0;
}

void vmm_free(page_directory_t *dir, uint32_t virt, uint32_t size) {
    if (!dir) {
        ERROR("[VMM]: DIRECTORY NOT GIVEN ABORTING\n");
        return;
    }

    uint32_t n_pages = size / PAGE_SIZE;
    DEBUG("[VMM]: Number of pages to be freed: %d\n", n_pages);
    for(uint32_t i = 0; i < n_pages; i++) {
        uint32_t phys_addr = paging_get_phys(dir, virt);
        pmm_free(phys_addr);
        paging_unmap(dir, virt);
        virt += PAGE_SIZE;
    }
    DEBUG("[VMM]: Pages freed\n");
}

void vmm_free_user_space(page_directory_t *dir) {
    
    DEBUG("[VMM]: Freeing userspace directory. \n");

    if (!dir) {
        ERROR("[VMM]: DIRECTORY NOT GIVEN ABORTING\n");
        return;
    }

    uint32_t size = KERNEL_VIRTUAL_BASE >> 22; //768
    uint32_t entries_per_page = PAGE_SIZE / 4;

    for(int i = 0; i < size; i++) {
        if(!((*dir)[i] & PAGE_PRESENT)) continue;
        uint32_t pt_phys = (*dir)[i] & ~0xFFF;
        uint32_t *pt = (uint32_t *)phys_to_virt(pt_phys);
        for(uint32_t j = 0; j < entries_per_page ; j++) {
            if(pt[j] & PAGE_PRESENT) {
                pmm_free(pt[j] & ~0xFFF);
            }
        }
        pmm_free(pt_phys);
        (*dir)[i] = 0;
    }

    DEBUG("[VMM]: Userspace directory freed\n");
}

void vmm_switch(page_directory_t *dir) {
    paging_switch(dir);
}

page_directory_t *vmm_create_directory(void) {
    return paging_create_directory();
}

uint32_t vmm_get_phys(page_directory_t *dir, uint32_t virt) {
    return paging_get_phys(dir, virt);
}