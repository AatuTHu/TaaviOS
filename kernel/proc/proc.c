#include "proc.h"
#include "config.h"
#include "kmalloc.h"
#include "vmm.h"
#include "kstring.h"

static proc_t *process_table[MAX_PROCESSES];

proc_t *process_create(uint32_t entry,const char *name) {

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

    page_directory_t *virt_dir = vmm_create_directory();

    int response = vmm_alloc(virt_dir, USER_STACK_TOP, USER_STACK_SIZE, PAGE_USER_RW);
    if(response != 0) return NULL;

    uint32_t kernel_stack = (uint32_t)kmalloc(KERNEL_STACK_SIZE);

    if(kernel_stack == NULL) return NULL;

    proc->pid            = slot;
    strncpy(proc->name, name, sizeof(proc->name));
    proc->state          = PROCESS_READY;
    proc->page_dir       = virt_dir;
    

    proc->kernel_stack   = kernel_stack + KERNEL_STACK_SIZE; //kernel stack is bottom of stack so add the stack on top of it

    proc->context.eip    = entry;
    proc->useresp        = USER_STACK_TOP;
    proc->context.ebp    = proc->useresp;
    proc->context.eflags = EFLAGS_DEFAULT;

    //for now before usermode
    proc->context.cs     = SEG_KERNEL_CODE; //SEG_USER_CODE;
    proc->context.ss     = SEG_KERNEL_DATA; //SEG_USER_DATA;
    
    process_table[slot]  = proc;

    return proc;
}

void process_destroy(proc_t *proc) {
    if(proc == NULL) return;
    process_table[proc->pid] = NULL;
    vmm_free(proc->page_dir, proc->useresp, USER_STACK_SIZE);
    kfree(proc->kernel_stack);
    kfree(proc->page_dir);
    kfree(proc);
}

proc_t *process_get(int index) {
    if(index >= 0 && index <= MAX_PROCESSES-1) {
        return process_table[index];
    }
    return NULL;
}