#include "vmm.h"
#include "config.h"
#include "klog.h"
#include "mm.h"
#include "pmm.h"
#include <stdint.h>

/**
 * vmm_alloc - Allocates pages to give directory.
 * @param *dir: target page directory.
 * @param virt: virtual addres.
 * @param size: size of the address space.
 * @param flags: -
 *
 * Description:
 * This function fills the page directory by allocating physical memory for it.
 * First it calculates how many pages it hast to allocate, then it loops the
 * number of pages over while mapping them also by using paging map function.
 * In a case of error it rolls back the allocated pages and frees them and
 * finally freeing the directory itself also.
 *
 * Context: This functions should be called after directory has been created
 * and in needs pages to it.
 * Return: STATUS_OK || STATUS_ERROR.
 */
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

        if (paging_map(dir, virt, addr, flags) == STATUS_OK) {
            virt += PAGE_SIZE;
            continue;
        }

        ERROR("[VMM]: Failed mapping the page. Aborting\n");
        if (pmm_free(paging_get_phys(dir, virt)) == STATUS_ERROR) {
            ERROR("[VMM]: Failed to free page directory\n");
        }

        return STATUS_ERROR;
    }
    // DEBUG_CORE_MM("[VMM]: ALLOCATION SUCCESSFUL\n");
    return STATUS_OK;
}

// This function is not used or has not been used in ages? Safe to delete?
int vmm_free(page_directory_t *dir, uint32_t virt, uint32_t size) {
    if (!dir) {
        ERROR("[VMM]: DIRECTORY NOT GIVEN ABORTING\n");
        return STATUS_ERROR;
    }

    uint32_t n_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    // DEBUG_CORE_MM("[VMM]: Number of pages to be freed: %d\n", n_pages);
    for (uint32_t i = 0; i < n_pages; i++) {
        uint32_t phys_addr = paging_get_phys(dir, virt);
        if (pmm_free(phys_addr) == STATUS_ERROR) {
            return STATUS_ERROR;
        }
        if (paging_unmap(dir, virt) == STATUS_ERROR) {
            return STATUS_ERROR;
        }
        virt += PAGE_SIZE;
    }
    // DEBUG_CORE_MM("[VMM]: Pages freed\n");
    return STATUS_OK;
}

/**
 * vmm_free_user_space - Called when page directory need to be freed.
 * @param *dir: target directory to be delete.
 *
 * Description:
 * This function takes the given page directory and completely tears down it.
 * First step is to calculate page dir entries and page table entries counts (hard coded)
 * Second step is to loop over page dir entries checking if the index of the dir is present
 * Third step is to masks the entry for the physical page table. Then changes it to virtual table
 * Fourth step is to loop over the page table entries by freeing the entries while masking the flags
 * Fifth step is to free the physical page table itself and set the index of the directory to be 0.
 * Sixth step after the loops is to free the entire page directory
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int vmm_free_user_space(page_directory_t *dir) {

    DEBUG_CORE_MM("[VMM]: Freeing userspace directory. \n");

    if (!dir) {
        ERROR("[VMM]: DIRECTORY NOT GIVEN ABORTING\n");
        return STATUS_ERROR;
    }

    uint32_t pde_count = KERNEL_VIRTUAL_BASE >> 22; // 768 page dir entires
    uint32_t pte_count = PAGE_SIZE / 4;             // page table entries

    DEBUG_CORE_MM("[VMM]: size: %d. \n", pde_count);
    DEBUG_CORE_MM("[VMM]: entries per page: %d. \n", pte_count);

    for (uint32_t i = 0; i < pde_count; i++) {
        if (!((*dir)[i] & PAGE_PRESENT)) {
            continue;
        }
        uint32_t pt_phys = (*dir)[i] & ~PAGE_FLAGS_MASK;

        if (pt_phys == 0) {
            ERROR("[VMM]: PDE entry %d marked present but points to phys 0x0!\n", i);
            (*dir)[i] = 0;
            continue;
        }
        // DEBUG_CORE_MM("[VMM]: pt_phys: %d. \n", pt_phys);
        const uint32_t *pt = (uint32_t *)phys_to_virt(pt_phys);
        for (uint32_t j = 0; j < pte_count; j++) {
            if (pt[j] & PAGE_PRESENT) {
                if (pmm_free(pt[j] & ~PAGE_FLAGS_MASK) == STATUS_ERROR) {
                    ERROR("[VMM]: Could not free page tables index\n");
                    continue;
                }
            }
        }
        if (pmm_free(pt_phys) == STATUS_ERROR) {
            ERROR("[VMM]: Could not free page table\n");
            return STATUS_ERROR;
        }
        (*dir)[i] = 0;
    }

    DEBUG_CORE_MM("[VMM]: Freeing physical page directory\n");
    if (pmm_free(virt_to_phys((uint32_t)dir)) == STATUS_ERROR) {
        ERROR("[VMM]: Failed to free physical page directory\n");
        return STATUS_ERROR;
    }

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

/**
 * vmm_alloc_kstack - Called when task needs a kernel_stack.
 * @param *task_out: allocated stack is stored to here.
 *
 * Description:
 * This function allocates a page by using pmm_alloc. It checks if the page
 * is valid and then changes it to virtual and adds size
 *
 * Return: (STATUS_OK && allocated kernel stack) || STATUS_ERROR.
 */
int vmm_alloc_kstack(uint32_t *stack_out) {
    uint32_t temp_stack = pmm_alloc();
    if (temp_stack == 0) {
        return STATUS_ERROR;
    }

    *stack_out = phys_to_virt(temp_stack) + KERNEL_STACK_SIZE;
    return STATUS_OK;
}

/**
 * vmm_free_kstack - Called when kernel stack needs to be freed.
 * @param kernel_stack: -
 *
 * Description:
 * This function frees the given stack.
 * First the stack is changed to physical address and its size is taken of it.
 * Then it is freed.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int vmm_free_kstack(uint32_t kernel_stack) {
    uint32_t phys = virt_to_phys(kernel_stack - KERNEL_STACK_SIZE);
    if (pmm_free(phys) == STATUS_ERROR) {
        return STATUS_ERROR;
    }
    return STATUS_OK;
}
