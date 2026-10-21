#include "vmm.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include "paging.h"
#include "pmm.h"
#include <stdint.h>

static uint64_t kstack_map[MAX_TASKS];

/**
 * vmm_alloc - Allocates pages to give directory.
 * @param *dir: target page directory.
 * @param virt: virtual addres.
 * @param size: size of the address space.
 * @param flags: -
 *
 * Description:
 * This functionion maps pages to page_directory at given virtual address the amount of size with the flags
 * provided. At first it checks that dir is provided, size is not zero and that virt is canonical.
 * After that it calculates how many pages the size needs. After thtat it loops over the an
 * allocates and maps pages to given virtual address. Should physical memory manager run out of
 * pages mid allocating the function start a rollback mech freeing the already allocated pages.
 *
 * Context: This functions should be called when one wants to allocate a space within the page
 * directory. For example heap memory.
 * Return: STATUS_OK || STATUS_ERROR.
 */
int vmm_alloc(page_directory_t *dir, uint64_t virt, uint64_t size, uint64_t flags) {

    if (!dir || size == 0 || (virt % PAGE_SIZE) != 0) {
        ERROR("[VMM]: DIRECTORY / VIRT_ADDR / SIZE INVALID. ABORTING\n");
        return STATUS_ERROR;
    }

    // DEBUG_CORE_MM("[VMM]: ALLOCATING VIRTUAL MEMORY\n");
    // DEBUG_CORE_MM("[VMM]: Virtual address: %x\n", virt);
    // DEBUG_CORE_MM("[VMM]: Size: %d\n", size);
    // DEBUG_CORE_MM("[VMM]: Flags: %d\n", flags);

    uint64_t n_pages    = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint64_t virt_start = virt;
    for (uint64_t i = 0; i < n_pages; i++) {
        uint64_t addr = pmm_alloc();

        if (addr == 0) {
            ERROR("[VMM]: NO PHYSICAL MEMORY LEFT FOR THE ALLOCATION\n");
            ERROR("[VMM]: STARTING ROLLBACK\n");
            while (virt > virt_start) {
                virt -= PAGE_SIZE;
                uint64_t phys_addr = paging_get_phys(dir, virt);
                if (phys_addr != INVALID_PHYSICAL_PAGE) {
                    pmm_free(phys_addr);
                }

                if (paging_unmap(dir, virt) == STATUS_ERROR) {
                    ERROR("[VMM]: Unable  to unmap pages\n");
                }
            }
            ERROR("[VMM]: ROLLBACK SUCCESSFUL\n");
            return STATUS_ERROR;
        }

        memset((void *)(addr + paging_hhdm), 0, PAGE_SIZE);

        if (paging_map(dir, virt, addr, flags) == STATUS_OK) {
            virt += PAGE_SIZE;
            continue;
        }

        ERROR("[VMM]: Failed mapping the page. Aborting\n");
        ERROR("[VMM]: STARTING ROLLBACK\n");
        while (virt > virt_start) {
            virt -= PAGE_SIZE;
            uint64_t phys_addr = paging_get_phys(dir, virt);
            if (phys_addr != INVALID_PHYSICAL_PAGE) {
                pmm_free(phys_addr);
            }

            if (paging_unmap(dir, virt) == STATUS_ERROR) {
                ERROR("[VMM]: Unable  to unmap pages\n");
            }
        }
        ERROR("[VMM]: ROLLBACK SUCCESSFUL\n");

        return STATUS_ERROR;
    }
    // DEBUG_CORE_MM("[VMM]: ALLOCATION SUCCESSFUL\n");

    return STATUS_OK;
}

int vmm_free(page_directory_t *dir, uint64_t virt, uint64_t size) {
    if (!dir) {
        ERROR("[VMM]: DIRECTORY NOT GIVEN ABORTING\n");
        return STATUS_ERROR;
    }

    uint64_t n_pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;
    // DEBUG_CORE_MM("[VMM]: Number of pages to be freed: %d\n", n_pages);
    for (uint64_t i = 0; i < n_pages; i++) {
        uint64_t phys_addr = paging_get_phys(dir, virt);
        if (phys_addr != INVALID_PHYSICAL_PAGE) {
            paging_unmap(dir, virt);
            pmm_free(phys_addr);
        }
        virt += PAGE_SIZE;
    }
    // DEBUG_CORE_MM("[VMM]: Pages freed\n");

    return STATUS_OK;
}

void vmm_switch(page_directory_t *dir) {
    paging_switch(dir);
}

uint64_t vmm_get_phys(page_directory_t *dir, uint64_t virt) {
    return paging_get_phys(dir, virt);
}

page_directory_t *vmm_create_dir(void) {
    return paging_create_directory();
}

/**
 * vmm_free_user_space - Called when page directory needs to be tearn down complitely.
 * @param *dir: target directory to be delete.
 *
 * Description:
 * This function walks from pml4 to pt table. Once there it frees every entry whitin it.
 * After that it moves one table up and frees everything in it aswell, then pdp and finally pml4.
 * Essentially freeing all present pages whiting the directory.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int vmm_free_user_space(page_directory_t *dir) {
    if (!dir) {
        return STATUS_ERROR;
    }

    uint64_t *pml4 = (uint64_t *)(uintptr_t)dir;

    for (int i = 0; i < KERNEL_PD_INDEX_START; i++) {
        if (!(pml4[i] & PAGE_PRESENT)) {
            continue;
        }

        uint64_t *pdpt = (uint64_t *)((pml4[i] & PAGE_FRAME_MASK) + paging_hhdm);

        for (int j = 0; j < ENTRIES_PER_TABLE; j++) {
            if (!(pdpt[j] & PAGE_PRESENT)) {
                continue;
            }

            uint64_t *pd = (uint64_t *)((pdpt[j] & PAGE_FRAME_MASK) + paging_hhdm);

            for (int k = 0; k < ENTRIES_PER_TABLE; k++) {
                if (!(pd[k] & PAGE_PRESENT)) {
                    continue;
                }

                uint64_t *pt = (uint64_t *)((pd[k] & PAGE_FRAME_MASK) + paging_hhdm);

                for (int l = 0; l < ENTRIES_PER_TABLE; l++) {
                    if (!(pt[l] & PAGE_PRESENT)) {
                        continue;
                    }

                    pmm_free(pt[l] & PAGE_FRAME_MASK);
                    pt[l] = 0;
                }

                pmm_free(pd[k] & PAGE_FRAME_MASK);
                pd[k] = 0;
            }

            pmm_free(pdpt[j] & PAGE_FRAME_MASK);
            pdpt[j] = 0;
        }

        pmm_free(pml4[i] & PAGE_FRAME_MASK);
        pml4[i] = 0;
    }

    return STATUS_OK;
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
int vmm_alloc_kstack(uint64_t *stack_out) {

    for (int i = 0; i < MAX_TASKS; i++) {
        if (kstack_map[i] == 0) {
            uint64_t base = KSTACK_REGION_BASE + i * KSTACK_SLOT_SIZE;
            if (vmm_alloc(&kernel_page_dir, base + PAGE_SIZE_BYTES, KERNEL_STACK_PAGES * PAGE_SIZE_BYTES, PAGE_RW | PAGE_NX) == STATUS_ERROR) {
                ERROR("[VMM][ALLOC_KSTACK]: Failed to map the kernel stack. Aborting\n");
                return STATUS_ERROR;
            }
            *stack_out    = base + KSTACK_SLOT_SIZE;
            kstack_map[i] = 1;
            return STATUS_OK;
        }
    }

    ERROR("[VMM][ALLOC_KSTACK]: Could not allocate kernel_stack\n");
    return STATUS_ERROR;
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
int vmm_free_kstack(uint64_t kernel_stack) {

    uint64_t base   = kernel_stack - KSTACK_SLOT_SIZE;
    uint64_t offset = base - KSTACK_REGION_BASE;
    uint64_t index  = offset / KSTACK_SLOT_SIZE;

    if (index >= MAX_TASKS) {
        ERROR("[VMM][FREE_KSTACK]: Stacks index on kstack_map is invalid\n");
        return STATUS_ERROR;
    }

    if (kstack_map[index] == 0) {
        ERROR("[VMM][FREE_KSTACK]: Stacks index on kstack_map is marked as free. Aborting\n");
        return STATUS_ERROR;
    }

    if (vmm_free(&kernel_page_dir, kernel_stack - KERNEL_STACK_PAGES * PAGE_SIZE_BYTES, KERNEL_STACK_PAGES * PAGE_SIZE_BYTES) == STATUS_ERROR) {
        ERROR("[VMM][FREE_KSTACK]: Failed to free stack\n");
        return STATUS_ERROR;
    }

    kstack_map[index] = 0;
    return STATUS_OK;
}
