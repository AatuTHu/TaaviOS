#include "proc.h"
#include "config.h"
#include "kmalloc.h"
#include "vmm.h"
#include "kstring.h"
#include "klog.h"
#include "pmm.h"
#include "mm.h"

static proc_t *process_table[MAX_PROCESSES];

proc_t *process_create(uint32_t entry, const char *name, page_directory_t *page_dir, uint8_t proc_mode) {

    int slot = -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] == NULL) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) return NULL;
    proc_t *proc = (proc_t *)kmalloc(sizeof(proc_t));

    if(proc == NULL) return NULL;

    page_directory_t *virt_dir = NULL;

    if(page_dir == NULL) {
        virt_dir = vmm_create_directory();
    } else {
        virt_dir = page_dir;
    }

    if(proc_mode == USER_PROCESS) {
        if(vmm_alloc(virt_dir, USER_STACK_TOP, USER_STACK_SIZE, PAGE_USER_RW) == STATUS_ERROR) return NULL;
    }

    uint32_t kernel_stack = pmm_alloc(); //allocate a physical page so that it is reality
    if(kernel_stack == 0) return NULL;

    memset(&proc->context, 0, sizeof(struct registers));
    
    proc->pid            = slot;
    proc->state          = PROCESS_READY;
    proc->page_dir       = virt_dir;
    proc->started        = 0;
    
    strncpy(proc->name, name, sizeof(proc->name));
    proc->kernel_stack   = phys_to_virt(kernel_stack) + KERNEL_STACK_SIZE; //kernel stack is bottom of stack so add the stack on top of it. Convert it to virtual addr

    proc->context.eip    = entry; //Begining of the proc like the main function
    proc->context.useresp= proc_mode == USER_PROCESS ? USER_STACK_TOP + USER_STACK_SIZE : proc->kernel_stack; //stack pointer?
    proc->priority       = proc_mode == USER_PROCESS ? PRIORITY_NORMAL : PRIORITY_LOW;
    proc->context.cs     = proc_mode == USER_PROCESS ? SEG_USER_CODE : SEG_KERNEL_CODE;
    proc->context.ss     = proc_mode == USER_PROCESS ? SEG_USER_DATA : SEG_KERNEL_DATA;
    proc->context.ebp    = proc->context.useresp; //stack bottom. Same as top in the begining. No plate you know
    proc->context.esp    = 0;//proc->context.useresp;
    proc->context.eflags = EFLAGS_DEFAULT;
    
    process_table[slot]  = proc;

    return proc;
}

void process_destroy(proc_t *proc, uint8_t proc_mode) {
    if(proc == NULL) return;
    DEBUG("[PROC]: Destroyn process: %s \n", proc->name);
    process_table[proc->pid] = NULL;
    DEBUG("[PROC]: Freeing virtual memory \n");
    proc_mode == USER_PROCESS ? vmm_free_user_space(proc->page_dir) : NULL;
    DEBUG("[PROC]: Freeing kernel_stack\n");
    uint32_t phys = virt_to_phys(proc->kernel_stack - KERNEL_STACK_SIZE);
    pmm_free(phys);
    DEBUG("[PROC]: Freeing physical page directory\n");
    pmm_free(virt_to_phys(proc->page_dir));
    DEBUG("[PROC]: Freeing process\n");
    kfree(proc);
}

proc_t *get_proc_by_name(char *name) {
    if (name == NULL) return NULL;

    for(int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] == NULL) {
            continue;
        }
        
        if (strcmp(process_table[i]->name, name) == 0) {
            return process_table[i];
        }
    }
    return NULL;
}
 
proc_t *process_get(int index) {
    if(index >= 0 && index <= MAX_PROCESSES-1) {
        return process_table[index];
    }
    return NULL;
}