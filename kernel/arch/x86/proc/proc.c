#include "proc.h"
#include "kmalloc.h"
#include "vmm.h"
#include "paging.h"
#include "config.h"

static process_t *process_table[MAX_PROCESSES];

process_t *process_create(uint32_t entry) {

    int slot = -1;

    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] == NULL) {
            slot = i;
            break;
        }
    }
    
    if (slot == -1) return NULL;
    process_t *proc = (process_t *)kmalloc(sizeof(process_t));

    if(proc == NULL) {return NULL;}

    page_directory_t *virt_dir = paging_create_directory();

    int response = vmm_alloc(virt_dir,USER_STACK_TOP,USER_STACK_SIZE, 0x200);
    if(response != 0) {return NULL;}

    uint32_t kernel_stack = (uint32_t)kmalloc(KERNEL_STACK_SIZE);

    if(kernel_stack == NULL) {return NULL;}

    proc->pid            = slot;
    proc->state          = PROCESS_READY;
    proc->page_dir       = virt_dir;
    
    /* * Stack Pointers:
     * esp: Points to the User Mode stack.
     * kernel_stack: Used by the TSS to land the CPU safely when a 
     * syscall or interrupt occurs while the process is running.
     */
    proc->user_stack     = USER_STACK_TOP;
    proc->kernel_stack   = kernel_stack;
    
    /* Initial Execution Context */
    proc->context.eip    = entry;
    proc->context.esp    = proc->user_stack;
    proc->context.ebp    = proc->user_stack;
    
    /* * WHY 0x202: Bit 9 is the IF (Interrupt Flag). 
     * We must start the process with interrupts enabled, otherwise the 
     * scheduler would never be able to preempt it.
     */
    proc->context.eflags = 0x202;

    /* * Segment Selectors:
     * 0x1b: User Code Segment (GDT index 3, RPL 3)
     * 0x23: User Data Segment (GDT index 4, RPL 3)
     */
    proc->cs             = 0x1b;
    proc->ss             = 0x23;
    
    process_table[slot]  = proc;

    return proc;
}

void process_destroy(process_t *proc) {
    if(proc == NULL) return;
    process_table[proc->pid] = NULL;
    vmm_free(proc->page_dir, proc->user_stack, USER_STACK_SIZE);
    kfree(proc->kernel_stack);
    kfree(proc->page_dir);
    kfree(proc);
}

process_t *process_get(int index) {
    if(index >= 0 && index <= 4) {
        return process_table[index];
    }
    return NULL;
}