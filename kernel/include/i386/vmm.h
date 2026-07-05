#ifndef VMM_H
#define VMM_H

#include "paging.h"
#include <stdint.h>

int vmm_alloc(page_directory_t *dir, uint32_t virt, uint32_t size,
    uint32_t flags);
void vmm_free(page_directory_t *dir, uint32_t virt, uint32_t size);
void vmm_switch(page_directory_t *dir);
uint32_t vmm_get_phys(page_directory_t *dir, uint32_t virt);
page_directory_t *vmm_create_directory(void);
int vmm_free_user_space(page_directory_t *dir);

#endif