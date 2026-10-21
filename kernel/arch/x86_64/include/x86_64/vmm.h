#ifndef VMM_H
#define VMM_H

#include "paging.h"
#include <stdint.h>

int vmm_alloc(page_directory_t *dir, uint64_t virt, uint64_t size, uint64_t flags);
int vmm_free(page_directory_t *dir, uint64_t virt, uint64_t size);
void vmm_switch(page_directory_t *dir);
uint64_t vmm_get_phys(page_directory_t *dir, uint64_t virt);
page_directory_t *vmm_create_dir(void);
int vmm_free_user_space(page_directory_t *dir);
int vmm_alloc_kstack(uint64_t *stack_out);
int vmm_free_kstack(uint64_t kernel_stack);

#endif
