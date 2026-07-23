#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define ENTRIES_PER_TABLE 1024
#define KERNEL_PD_INDEX_START 768
#define KERNEL_PD_ENTRIES 256
#define PAGE_SHIFT 12
#define PAGE_SIZE_BYTES 4096
#define PT_INDEX_MASK 0x3FF
#define PD_INDEX_SHIFT 22
#define PAGE_FLAGS_MASK 0xFFF
#define PAGE_FRAME_MASK 0xFFFFF000
#define PAGE_PRESENT (1 << 0)
#define PAGE_RW (1 << 1)
#define PAGE_USER (1 << 2)
#define PAGE_USER_RW (PAGE_PRESENT | PAGE_RW | PAGE_USER)

typedef uint32_t page_directory_t[1024];
typedef uint32_t page_table_t[1024];
extern page_directory_t kernel_page_dir;

typedef struct deferred_map_entry {
    page_directory_t *dir;
    uint32_t virt;
    uint32_t phys;
    uint32_t flags;
    uint32_t page_count;
    uint8_t taken;
} deferred_map_entry_t;

extern int switch_page_dir(uint32_t addr);
int paging_add_deferred_mapping(page_directory_t *dir, uint32_t virt, uint32_t phys, uint32_t flags, uint32_t page_count);
void paging_init(void);
int paging_map(page_directory_t *dir, uint32_t virt, uint32_t phys,
               uint32_t flags);
void paging_unmap(page_directory_t *dir, uint32_t virt);
void paging_switch(page_directory_t *dir);
page_directory_t *paging_create_directory(void);
uint32_t paging_get_phys(page_directory_t *dir, uint32_t virt);

#endif
