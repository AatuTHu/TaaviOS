#ifndef PAGING_H
#define PAGING_H

#include "boot_info.h"
#include <stdint.h>

#define ENTRIES_PER_TABLE 512
#define KERNEL_PD_INDEX_START 256
#define KERNEL_PD_ENTRIES 256
#define PAGE_SHIFT 12
#define PAGE_SIZE_BYTES 4096ULL
#define HUGE_PAGE_SIZE_BYTES (2ULL * 1024 * 1024)
#define PT_INDEX_MASK 0x1FF
#define PML4_SHIFT 39
#define PDP_SHIFT 30
#define PD_SHIFT 21
#define PT_SHIFT 12
#define PAGE_FRAME_MASK 0x000FFFFFFFFFF000ULL
#define PAGE_FLAGS_MASK (~PAGE_FRAME_MASK)

#define PAGE_PRESENT (1ULL << 0)
#define PAGE_RW (1ULL << 1)
#define PAGE_USER (1ULL << 2)
#define PAGE_PWT (1ULL << 3)
#define PAGE_PCD (1ULL << 4)
#define PAGE_HUGE (1ULL << 7)
#define PAGE_GLOBAL (1ULL << 8)
#define PAGE_NX (1ULL << 63)
#define PAGE_USER_RW (PAGE_PRESENT | PAGE_RW | PAGE_USER)

typedef uint64_t page_directory_t[ENTRIES_PER_TABLE];
typedef uint64_t page_table_t[ENTRIES_PER_TABLE];
extern page_directory_t kernel_page_dir;
extern uint64_t paging_hhdm;
static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(phys + paging_hhdm);
}

extern void switch_page_dir(uint64_t addr);
int paging_init(const struct boot_info *bi);
int paging_map(page_directory_t *dir, uint64_t virt, uint64_t phys, uint64_t flags);
int paging_map_huge(page_directory_t *dir, uint64_t virt, uint64_t phys, uint64_t flags);
int paging_unmap(page_directory_t *dir, uint64_t virt);
void paging_switch(page_directory_t *dir);
page_directory_t *paging_create_directory(void);
uint64_t paging_get_phys(page_directory_t *dir, uint64_t virt);

#endif
