#include "proc.h"
#include "config.h"
#include "kmalloc.h"
#include "vmm.h"
#include "kstring.h"
#include "klog.h"
#include "pmm.h"
#include "mm.h"

static proc_t *process_table[MAX_PROCESSES];

proc_t *process_create(uint32_t entry, const char *name, page_directory_t *page_dir) {

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
        virt_dir = paging_create_directory();
    } else {
        virt_dir = page_dir;
    }

    int response = vmm_alloc(virt_dir, USER_STACK_TOP, USER_STACK_SIZE, PAGE_USER_RW);
    if(response != 0) return NULL;

    uint32_t kernel_stack = pmm_alloc(); //allocate a physical page so that it is reality

    if(kernel_stack == 0) return NULL;

    memset(&proc->context, 0, sizeof(struct registers));
    
    proc->pid            = slot;
    strncpy(proc->name, name, sizeof(proc->name));
    proc->state          = PROCESS_READY;
    proc->priority       = PRIORITY_NORMAL;
    proc->page_dir       = virt_dir;
    proc->started        = 0;

    proc->kernel_stack   = phys_to_virt(kernel_stack) + KERNEL_STACK_SIZE; //kernel stack is bottom of stack so add the stack on top of it. Convert it to virtual addr

    proc->context.eip    = entry; //Begining of the proc like the main function
    proc->context.useresp= USER_STACK_TOP + USER_STACK_SIZE; //stack pointer?
    proc->context.esp    = 0;//proc->context.useresp;
    proc->context.ebp    = proc->context.useresp; //stack bottom. Same as top in the begining. No plate you know
    proc->context.eflags = EFLAGS_DEFAULT;
    proc->context.cs     = SEG_USER_CODE;
    proc->context.ss     = SEG_USER_DATA;
    
    process_table[slot]  = proc;

    return proc;
}

void process_destroy(proc_t *proc) {
    if(proc == NULL) return;
    DEBUG("[PROC]: Destroyn process: %s \n", proc->name);
    process_table[proc->pid] = NULL;
    DEBUG("[PROC]: Freeing virtual memory \n");
    vmm_free(proc->page_dir, USER_STACK_TOP, USER_STACK_SIZE);
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