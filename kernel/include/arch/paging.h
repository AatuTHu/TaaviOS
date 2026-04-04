#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

typedef uint32_t page_directory_t[1024];
typedef uint32_t page_table_t[1024];
extern page_directory_t kernel_page_dir;

void paging_init(void);
void paging_map(page_directory_t *dir, uint32_t virt, uint32_t phys, uint32_t flags);
void paging_unmap(page_directory_t *dir, uint32_t virt);
void paging_switch(page_directory_t *dir);
page_directory_t *paging_create_directory(void);
uint32_t paging_get_phys(page_directory_t *dir, uint32_t virt);

#endif