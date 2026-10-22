#include "paging.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include "pmm.h"
#include <stdint.h>

/**
 * Paging
 * Design & Implementation:
 * @author: A.H, 2026
 */

page_directory_t kernel_page_dir __attribute__((aligned(PAGE_SIZE_BYTES)));
uint64_t paging_hhdm;
uint64_t kernel_physical_base;
uint64_t kernel_virtual_base;

/**
 * __search_or_create_table - Used to descend tables. Can create an entry if wanted.
 * @table: table level from where to look up/create an entry.
 * @index: target entry withing the table.
 * @create: 1 = create new entry, 0 = don't create
 * @flags: holds flags for the new entry
 *
 * Description:
 * At firs this functions tries to look if the entry at the given index is present. If it is it then
 * checks that the entry is not set with page_huge, because those cant descended to. If the entry is
 * not set to page_huge it checks that if given flags have page_user bit set and the entry does not.
 * If it is true the bit is set for the entry and then the virtual address of the entry is returned.
 *
 * If entry was not present and create variable is 0 the function returns null. If create is 1 The
 * function creates the entry to the given index position by asking for a 4kib page from physical
 * memory manager, zeroing it, setting appropriate flags and then returning the virtual address.
 *
 * Return: VIRTUAL ADDRESS OF FOUND ENTRY || NULL.
 */
static uint64_t *__search_or_create_table(uint64_t *table, int index, int create, uint64_t flags) {
    if (table[index] & PAGE_PRESENT) {
        if (table[index] & PAGE_HUGE) {
            return NULL;
        }

        if (flags & PAGE_USER && !(table[index] & PAGE_USER)) {
            table[index] |= PAGE_USER;
        }
        return phys_to_virt(table[index] & PAGE_FRAME_MASK);
    }

    if (!create) {
        return NULL;
    }

    DEBUG_CORE_MM("[PAGING][NEXT_TABLE]: Table not found, Creating it\n");
    uint64_t phys_page = pmm_alloc();

    if (phys_page == 0) {
        ERROR("[PAGING][NEXT_TABLE]: Encountered error while asking for a physical page. Aborting\n");
        return NULL;
    }

    memset(phys_to_virt(phys_page), 0, PAGE_SIZE);

    uint64_t entry_flags = flags & PAGE_USER ? PAGE_USER_RW : PAGE_PRESENT | PAGE_RW;

    table[index]         = phys_page | entry_flags;
    return phys_to_virt(table[index] & PAGE_FRAME_MASK);
}

/**
 * paging_map - maps new entry to given page directory.
 * @dir: target directory.
 * @virt: virtual address from where to start descending.
 * @phys: new page to be mapped
 * @flags: flags for the new page
 *
 * Description:
 * Firs this function checks that the virtual and the physical addresses are aligned to start 4kib
 * and that the virtual address is canonical. If these are a pass it then descends to page table by
 * using __search_or_create_table helper function creating the tables on the way there. Then the new
 * page is stored to page table with the given flags set on. After that it flushes tlb
 *
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int paging_map(page_directory_t *dir, uint64_t virt, uint64_t phys, uint64_t flags) {
    if ((virt & (PAGE_SIZE - 1)) != 0 || (phys & (PAGE_SIZE - 1)) != 0) {
        ERROR("[PAGING][PAGING_MAP]: Invalide virtaul or physical address\n");
        return STATUS_ERROR;
    }

    int64_t sign_extended = ((int64_t)virt << 16) >> 16;
    if (sign_extended != (int64_t)virt) {
        ERROR("[PAGING][PAGING_MAP]: Address not canonical\n");
        return STATUS_ERROR;
    }

    uint64_t *pdp_table = __search_or_create_table((uint64_t *)dir,
                                                   ((virt >> PML4_SHIFT) & PT_INDEX_MASK), 1, flags);
    if (pdp_table == NULL) {
        ERROR("[PAGING][PAGING_MAP]: PDP NULL, aborting\n");
        return STATUS_ERROR;
    }
    uint64_t *pd_table = __search_or_create_table(pdp_table,
                                                  ((virt >> PDP_SHIFT) & PT_INDEX_MASK), 1, flags);

    if (pd_table == NULL) {
        ERROR("[PAGING][PAGING_MAP]: PD NULL, aborting\n");
        return STATUS_ERROR;
    }
    uint64_t *pt_table = __search_or_create_table(pd_table,
                                                  ((virt >> PD_SHIFT) & PT_INDEX_MASK), 1, flags);

    if (pt_table == NULL) {
        ERROR("[PAGING][PAGING_MAP]: PT NULL, aborting\n");
        return STATUS_ERROR;
    }

    uint64_t pt_index = (virt >> PT_SHIFT) & PT_INDEX_MASK;

    if ((pt_table[pt_index] & PAGE_PRESENT) != 0) {
        ERROR("[PAGING][PAGING_MAP]: PT page not present, aborting\n");
        return STATUS_ERROR;
    }

    pt_table[pt_index] = phys | flags | PAGE_PRESENT;

    __asm__ __volatile__("invlpg (%0)" ::"r"(virt) : "memory");
    return STATUS_OK;
}

/**
 * paging_map_huge - maps huge entry to given page directory.
 * @dir: target directory.
 * @virt: virtual address from where to start descending.
 * @phys: new page to be mapped
 * @flags: flags for the new page
 *
 * Description:
 * Firs this function checks that the virtual and the physical addresses are aligned to start at 2mb
 * and that the virtual address is canonical. If these are a pass it then descends to page directory by
 * using __search_or_create_table helper function creating the tables on the way there. Then the new
 * entry is store to page directory with the given flags set on. After that it flushes tlb
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int paging_map_huge(page_directory_t *dir, uint64_t virt, uint64_t phys, uint64_t flags) {
    if ((virt & (HUGE_PAGE_SIZE_BYTES - 1)) != 0 || (phys & (HUGE_PAGE_SIZE_BYTES - 1)) != 0) {
        ERROR("[PAGING][PAGING_MAP]: Invalide virtaul or physical address\n");
        return STATUS_ERROR;
    }

    int64_t sign_extended = ((int64_t)virt << 16) >> 16;
    if (sign_extended != (int64_t)virt) {
        ERROR("[PAGING][PAGING_MAP_HUGE]: Address not canonical\n");
        return STATUS_ERROR;
    }
    uint64_t *pdp_table = __search_or_create_table((uint64_t *)dir,
                                                   ((virt >> PML4_SHIFT) & PT_INDEX_MASK), 1, flags);
    if (pdp_table == NULL) {
        ERROR("[PAGING][PAGING_MAP_HUGE]: PDP NULL, aborting\n");
        return STATUS_ERROR;
    }
    uint64_t *pd_table = __search_or_create_table(pdp_table,
                                                  ((virt >> PDP_SHIFT) & PT_INDEX_MASK), 1, flags);

    if (pd_table == NULL) {
        ERROR("[PAGING][PAGING_MAP_HUGE]: PD NULL, aborting\n");
        return STATUS_ERROR;
    }

    uint64_t pd_index = (virt >> PD_SHIFT) & PT_INDEX_MASK;

    if ((pd_table[pd_index] & PAGE_PRESENT) != 0) {
        ERROR("[PAGING][PAGING_MAP_HUGE]: Page table not present, aborting\n");
        return STATUS_ERROR;
    }
    pd_table[pd_index] = phys | flags | PAGE_PRESENT | PAGE_HUGE;

    __asm__ __volatile__("invlpg (%0)" ::"r"(virt) : "memory");
    return STATUS_OK;
}

/**
 * paging_unmap - Used to null a entry at the page table level.
 * @dir: target directory.
 * @virt: virtual address from where to start descending.
 *
 * Description:
 * This function checks that the virt address is aligned to 4kb and descends to page table. If one
 * of the tables on the way is a null it return early with an error. Otherwise the entry at page
 * table is removed.
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int paging_unmap(page_directory_t *dir, uint64_t virt) {
    if ((virt & (PAGE_SIZE - 1)) != 0) {
        ERROR("[PAGING][PAGING_UNMAP]: Virtual address not aligned, aborting\n");
        return STATUS_ERROR;
    }

    uint64_t *pdp_table = __search_or_create_table((uint64_t *)dir,
                                                   ((virt >> PML4_SHIFT) & PT_INDEX_MASK), 0, 0);
    if (pdp_table == NULL) {
        ERROR("[PAGING][PAGING_UNMAP]: PDP NULL, aborting\n");
        return STATUS_ERROR;
    }
    uint64_t *pd_table = __search_or_create_table(pdp_table,
                                                  ((virt >> PDP_SHIFT) & PT_INDEX_MASK), 0, 0);

    if (pd_table == NULL) {
        ERROR("[PAGING][PAGING_UNMAP]: PD NULL, aborting\n");
        return STATUS_ERROR;
    }
    uint64_t *pt_table = __search_or_create_table(pd_table,
                                                  ((virt >> PD_SHIFT) & PT_INDEX_MASK), 0, 0);

    if (pt_table == NULL) {
        ERROR("[PAGING][PAGING_UNMAP]: PT NULL, aborting\n");
        return STATUS_ERROR;
    }

    uint64_t pt_index = (virt >> PT_SHIFT) & PT_INDEX_MASK;

    if ((pt_table[pt_index] & PAGE_PRESENT) == 0) {
        ERROR("[PAGING][PAGING_UNMAP]: Page not present, aborting\n");
        return STATUS_ERROR;
    }

    pt_table[(virt >> PT_SHIFT) & PT_INDEX_MASK] = 0;

    __asm__ __volatile__("invlpg (%0)" ::"r"(virt) : "memory");
    return STATUS_OK;
}

/**
 * paging_get_phys - descends to page table and gives the physical address of the entry there.
 * @dir: target directory.
 * @virt: virtual address from where to start descending.
 *
 * Description:
 * This function descends thru the given dir checking that each index is present. Finally returning
 * the physical address of the page at page table level
 *
 * Return: PAGE_TABLE ENTRY PHYSICAL ADDRESS || 0.
 */
uint64_t paging_get_phys(page_directory_t *dir, uint64_t virt) {
    uint64_t *table = (uint64_t *)dir;
    uint64_t index  = (virt >> PML4_SHIFT) & PT_INDEX_MASK;
    uint64_t entry  = table[index];

    if ((entry & PAGE_PRESENT) == 0) {
        ERROR("[PAGING][PGP]: No entry found at PML4\n");
        return 0;
    }

    table = (uint64_t *)phys_to_virt(entry & PAGE_FRAME_MASK);
    index = (virt >> PDP_SHIFT) & PT_INDEX_MASK;
    entry = table[index];

    if ((entry & PAGE_PRESENT) == 0) {
        ERROR("[PAGING][PGP]: No entry found at PDP\n");
        return 0;
    }

    table = (uint64_t *)phys_to_virt(entry & PAGE_FRAME_MASK);
    index = (virt >> PD_SHIFT) & PT_INDEX_MASK;
    entry = table[index];

    if ((entry & PAGE_PRESENT) == 0) {
        ERROR("[PAGING][PGP]: No entry found at PD\n");
        return 0;
    }

    if ((entry & PAGE_HUGE) != 0) {
        return (entry & PAGE_FRAME_MASK) + (virt & (HUGE_PAGE_SIZE_BYTES - 1));
    }

    table = (uint64_t *)phys_to_virt(entry & PAGE_FRAME_MASK);
    index = (virt >> PT_SHIFT) & PT_INDEX_MASK;
    entry = table[index];

    if ((entry & PAGE_PRESENT) == 0) {
        ERROR("[PAGING][PGP]: No entry found at PT\n");
        return 0;
    }

    return (entry & PAGE_FRAME_MASK) + (virt & 0xFFF);
}

/**
 * paging_create_directory - Creates a top level directory.
 *
 * Description:
 * This function asks for a 4kib page from pmm. Then the it add paging_higher_half_direct_map value to the
 * physical address and casts it to be a type of page_directory_t. After that the new directory is
 * zeroed and the kernel directories upper half is copied to the new directory.
 *
 * Return: new page_directory_t || null.
 */
page_directory_t *paging_create_directory(void) {
    uint64_t phys_page = pmm_alloc();
    if (phys_page == 0) {
        ERROR("[PAGING][PCD]: Unable to allocate a page, aborting\n");
        return NULL;
    }
    page_directory_t *virt_page = (page_directory_t *)(phys_page + paging_hhdm);
    memset(virt_page, 0, PAGE_SIZE);

    memcpy(&(*virt_page)[KERNEL_PD_INDEX_START],
           &kernel_page_dir[KERNEL_PD_INDEX_START],
           KERNEL_PD_ENTRIES * sizeof(uint64_t));

    return virt_page;
}

/**
 * paging_switch - flushes cr3 to hold the given directory.
 * @dir: target directory.
 *
 * Description:
 * First if directory is null the function return early. After that the dir is cast to be type of
 * uint64_t. Then if the address is above the virtual base the physical address is calculated by
 * subtracting from the address the value of virt_base + physical_base. Otherwise we subtract the
 * paging_higher_half_direct_map value from the address and send it to asm stub that sets it to cr3
 * register.
 *
 * Return:VOID
 */
void paging_switch(page_directory_t *dir) {

    if (!dir) {
        return;
    }
    uint64_t addr = (uint64_t)dir;
    if (addr >= kernel_virtual_base) {
        DEBUG_CORE_MM("[PAGING][PAGING_SWITCH]: Address over the virtual base 0x%x\n", addr - kernel_virtual_base + kernel_physical_base);
        addr = addr - kernel_virtual_base + kernel_physical_base;
    } else {
        DEBUG_CORE_MM("[PAGING][PAGING_SWITCH]: Address below virtual base 0x%x\n", addr - paging_hhdm);
        addr = addr - paging_hhdm;
    }

    switch_page_dir(addr);
}

/**
 * paging_init - initializes paging for the processor.
 * @bi: holds information about memory given by the limine bootloader.
 *
 * Description:
 * This functions starts by saving hhdm_offset, kernel_phys_base and kernel_virt_base to global
 * static variables. After that we read msr register to know if NX bit is set, if not we set it on
 * because otherwise the processor would not know from which parts of the memory is executable.
 *
 * After that we create kernel directory, zero it and fill it with pages with flags present and
 * read&write.
 *
 * Then we take each section .text, .rodata, .data and loop from section start to section end to the newly created kernel directory
 * with flags each section are allowed to have. For .text we set only flag present,
 * for .rodata we set present and executable, for .data we set present, executable and read&write.
 *
 * Then we move on to map all the memory regions. First we align them to be 2mb. Then we Add to the
 * physical address the hhdm value to get virtual address and map them by using paging_map_huge with
 * flags present, huge, rw, global and exectuable.
 *
 * lastly we switch the kernel directory on via paging_switch function
 *
 * Return: STATUS_OK || STATUS_ERROR.
 */
int paging_init(const struct boot_info *bi) {
    DEBUG_CORE_MM("[PAGING][INIT]: Initializing paging\n");
    paging_hhdm          = bi->hhdm_offset;
    kernel_physical_base = bi->kernel_phys_base;
    kernel_virtual_base  = bi->kernel_virt_base;
    uint32_t eax, edx;
    __asm__ volatile("rdmsr" : "=a"(eax), "=d"(edx) : "c"(0xC0000080));

    if (!(eax & (1 << 11))) {
        eax |= (1 << 11);
        __asm__ volatile("wrmsr" : : "c"(0xC0000080), "a"(eax), "d"(edx));
    }
    memset(&kernel_page_dir, 0, PAGE_SIZE);
    for (int i = KERNEL_PD_INDEX_START; i < ENTRIES_PER_TABLE; i++) {
        uint64_t phys_page = pmm_alloc();
        if (phys_page == 0) {
            ERROR("[PAGING][INIT]: Physical memory manager is out of pages\n");
            return STATUS_ERROR;
        }
        memset((void *)(phys_page + paging_hhdm), 0, PAGE_SIZE);
        kernel_page_dir[i] = phys_page | PAGE_PRESENT | PAGE_RW;
    }
    extern char _text_start[], _text_end[];
    extern char _rodata_start[], _rodata_end[];
    extern char _data_start[], _data_end[];

    for (uint64_t v = (uint64_t)(_text_start); v < (uint64_t)(_text_end); v += PAGE_SIZE) {
        uint64_t p = bi->kernel_phys_base + (v - bi->kernel_virt_base);
        if (paging_map(&kernel_page_dir, v, p, PAGE_PRESENT) == STATUS_ERROR) {
            ERROR("[PAGING][INIT]: failed to map text area, aborting\n");
            return STATUS_ERROR;
        }
    }

    for (uint64_t v = (uint64_t)(_rodata_start); v < (uint64_t)(_rodata_end); v += PAGE_SIZE) {
        uint64_t p = bi->kernel_phys_base + (v - bi->kernel_virt_base);
        if (paging_map(&kernel_page_dir, v, p, PAGE_PRESENT | PAGE_NX) == STATUS_ERROR) {
            ERROR("[PAGING][INIT]: failed to map rodata area, aborting\n");
            return STATUS_ERROR;
        }
    }

    for (uint64_t v = (uint64_t)(_data_start); v < (uint64_t)(_data_end); v += PAGE_SIZE) {
        uint64_t p = bi->kernel_phys_base + (v - bi->kernel_virt_base);
        if (paging_map(&kernel_page_dir, v, p, PAGE_PRESENT | PAGE_RW | PAGE_NX) == STATUS_ERROR) {
            ERROR("[PAGING][INIT]: failed to map data area, aborting\n");
            return STATUS_ERROR;
        }
    }

    for (size_t i = 0; i < bi->region_count; i++) {
        const struct boot_mem_region *entry = &bi->regions[i];
        if (entry->length == 0)
            continue;

        uint64_t base = entry->base & ~(HUGE_PAGE_SIZE_BYTES - 1);
        uint64_t end  = (entry->base + entry->length + HUGE_PAGE_SIZE_BYTES - 1) & ~(HUGE_PAGE_SIZE_BYTES - 1);

        for (uint64_t phys = base; phys < end; phys += HUGE_PAGE_SIZE_BYTES) {
            uint64_t virt     = phys + paging_hhdm;

            uint64_t pml4_idx = (virt >> PML4_SHIFT) & PT_INDEX_MASK;
            uint64_t pdp_idx  = (virt >> PDP_SHIFT) & PT_INDEX_MASK;
            uint64_t pd_idx   = (virt >> PD_SHIFT) & PT_INDEX_MASK;

            uint64_t *pml4    = (uint64_t *)&kernel_page_dir;
            if (pml4[pml4_idx] & PAGE_PRESENT) {
                uint64_t *pdp = phys_to_virt(pml4[pml4_idx] & PAGE_FRAME_MASK);
                if (pdp[pdp_idx] & PAGE_PRESENT) {
                    uint64_t *pd = phys_to_virt(pdp[pdp_idx] & PAGE_FRAME_MASK);
                    if (pd[pd_idx] & PAGE_PRESENT) {
                        continue;
                    }
                }
            }

            if (paging_map_huge(&kernel_page_dir, virt, phys,
                                PAGE_PRESENT | PAGE_RW | PAGE_NX | PAGE_HUGE | PAGE_GLOBAL) == STATUS_ERROR) {
                ERROR("[PAGING][INIT]: Failed to map region %d\n", (int)i);
                return STATUS_ERROR;
            }
        }
    }

    paging_switch(&kernel_page_dir);
    DEBUG_CORE_MM("[PAGING][INIT]: Paging initialized sucesffully\n");
    return STATUS_OK;
}
