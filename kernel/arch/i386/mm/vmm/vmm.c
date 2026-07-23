#include "vmm.h"
#include "config.h"
#include "klog.h"
#include "mm.h"
#include "pmm.h"

int vmm_alloc(page_directory_t *dir, uint32_t virt, uint32_t size,
              uint32_t flags) {
    if (!dir) {
        ERROR("[VMM]: DIRECTORY NOT GIVEN ABORTING ALLOCATION\n");
        return STATUS_ERROR;
    }

    // DEBUG_CORE_MM("[VMM]: ALLOCATING VIRTUAL MEMORY\n");
    // DEBUG_CORE_MM("[VMM]: Virtual address: %x\n", virt);
    // DEBUG_CORE_MM("[VMM]: Size: %d\n", size);
    // DEBUG_CORE_MM("[VMM]: Flags: %d\n", flags);

    uint32_t n_pages    = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    // DEBUG_CORE_MM("[VMM]: Number of pages to be allocated: %d\n", n_pages);
    uint32_t virt_start = virt;
    for (uint32_t i = 0; i < n_pages; i++) {
        uint32_t addr = pmm_alloc();
        if (addr == 0) {
            ERROR("[VMM]: NO PHYSICAL MEMORY LEFT FOR THE ALLOCATION\n");
            ERROR("[VMM]: STARTING ROLLBACK\n");
            while (virt > virt_start) {
                virt -= PAGE_SIZE;
                uint32_t phys_addr = paging_get_phys(dir, virt);
                if (phys_addr != INVALID_PHYSICAL_PAGE) {
                    pmm_free(phys_addr);
                }
                paging_unmap(dir, virt);
            }
            ERROR("[VMM]: ROLLBACK SUCCESSFUL\n");
            return STATUS_ERROR;
        }
        if (paging_map(dir, virt, addr, flags) == STATUS_ERROR) {
            ERROR("[VMM]: Failed mapping the page. Aborting\n");
            pmm_free(paging_get_phys(dir, virt));
            return STATUS_ERROR;
        }
        virt += PAGE_SIZE;
    }
    // DEBUG_CORE_MM("[VMM]: ALLOCATION SUCCESSFUL\n");
    return STATUS_OK;
}

void vmm_free(page_directory_t *dir, uint32_t virt, uint32_t size) {
    if (!dir) {
        ERROR("[VMM]: DIRECTORY NOT GIVEN ABORTING\n");
        return;
    }

    uint32_t n_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    // DEBUG_CORE_MM("[VMM]: Number of pages to be freed: %d\n", n_pages);
    for (uint32_t i = 0; i < n_pages; i++) {
        uint32_t phys_addr = paging_get_phys(dir, virt);
        pmm_free(phys_addr);
        paging_unmap(dir, virt);
        virt += PAGE_SIZE;
    }
    // DEBUG_CORE_MM("[VMM]: Pages freed\n");
}

int vmm_free_user_space(page_directory_t *dir) {

    DEBUG_CORE_MM("[VMM]: Freeing userspace directory. \n");

    if (!dir) {
        ERROR("[VMM]: DIRECTORY NOT GIVEN ABORTING\n");
        return STATUS_ERROR;
    }

    uint32_t size             = KERNEL_VIRTUAL_BASE >> 22; // 768
    uint32_t entries_per_page = PAGE_SIZE / 4;

    DEBUG_CORE_MM("[VMM]: size: %d. \n", size);
    DEBUG_CORE_MM("[VMM]: entries per page: %d. \n", entries_per_page);
    __asm__ __volatile__("cli");
    for (uint32_t i = 0; i < size; i++) {
        if (!((*dir)[i] & PAGE_PRESENT)) {
            continue;
        }
        uint32_t pt_phys = (*dir)[i] & ~PAGE_FLAGS_MASK;
        DEBUG_CORE_MM("[VMM]: pt_phys: %d. \n", pt_phys);
        const uint32_t *pt = (uint32_t *)phys_to_virt(pt_phys);
        for (uint32_t j = 0; j < entries_per_page; j++) {
            if (pt[j] & PAGE_PRESENT) {
                pmm_free(pt[j] & ~PAGE_FLAGS_MASK);
            }
        }
        pmm_free(pt_phys);
        (*dir)[i] = 0;
    }
    __asm__ __volatile__("sti");

    DEBUG_CORE_MM("[VMM]: Userspace directory freed\n");
    return STATUS_OK;
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
