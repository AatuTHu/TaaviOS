#ifndef VMM_H
#define VMM_H

#include <stdint.h>

typedef uint32_t page_directory_t[1024];

int vmm_alloc(page_directory_t *dir, uint32_t virt, uint32_t size, uint32_t flags);
void vmm_free(page_directory_t *dir, uint32_t virt, uint32_t size);
void vmm_switch(page_directory_t *dir);

#endif