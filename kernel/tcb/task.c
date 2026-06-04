#include "task.h"
#include "config.h"
#include "kmalloc.h"
#include "vmm.h"
#include "kstring.h"
#include "klog.h"
#include "pmm.h"
#include "mm.h"

static task_t *task_table[MAX_TASKS];

task_t *task_create(int reserved_pid, uint32_t entry, const char *name, page_directory_t *page_dir, uint8_t task_mode) {

    int slot = -1;

    if(reserved_pid != -1) {
        slot = reserved_pid;
    } else {
        for (int i = 0; i < MAX_TASKS; i++) {
            if (task_table[i] == NULL) {
                slot = i;
                break;
            }
        }
    }

    if (slot == -1) return NULL;
    
    task_t *task = (task_t *)kmalloc(sizeof(task_t));

    if(task == NULL) return NULL;

    page_directory_t *virt_dir = NULL;

    if(page_dir == NULL) {
        virt_dir = vmm_create_directory();
    } else {
        virt_dir = page_dir;
    }

    if(task_mode == USER_TASK) {
        if(vmm_alloc(virt_dir, USER_STACK_TOP, USER_STACK_SIZE, PAGE_USER_RW) == STATUS_ERROR) return NULL;
    }

    uint32_t kernel_stack = pmm_alloc(); //allocate a physical page so that it is reality
    if(kernel_stack == 0) return NULL;

    memset(&task->context, 0, sizeof(struct registers));
    
    task->pid            = slot;
    task->state          = task_mode == USER_TASK ? TASK_READY : TASK_SLEEPING;
    task->page_dir       = virt_dir;
    task->started        = 0;
    
    strncpy(task->name, name, sizeof(task->name));
    task->kernel_stack   = phys_to_virt(kernel_stack) + KERNEL_STACK_SIZE; //kernel stack is bottom of stack so add the stack on top of it. Convert it to virtual addr

    task->context.eip    = entry; //Begining of the task like the main function
    task->context.useresp= task_mode == USER_TASK ? USER_STACK_TOP + USER_STACK_SIZE : task->kernel_stack; //stack pointer?
    task->priority       = task_mode == USER_TASK ? PRIORITY_NORMAL : PRIORITY_LOW;
    task->context.cs     = task_mode == USER_TASK ? SEG_USER_CODE : SEG_KERNEL_CODE;
    task->context.ss     = task_mode == USER_TASK ? SEG_USER_DATA : SEG_KERNEL_DATA;
    task->context.ebp    = task->context.useresp; //stack bottom. Same as top in the begining. No plate you know
    task->context.esp    = task->context.useresp;
    task->context.eflags = EFLAGS_DEFAULT;
    task->task_mode      = task_mode; //can be usefull later?
    
    task_table[slot]  = task;

    return task;
}

void task_destroy(task_t *task, uint8_t task_mode) {
    if(task == NULL) return;

    DEBUG("[TASK]: Destroyn task: %s \n", task->name);
    task_table[task->pid] = NULL;

    DEBUG("[TASK]: Freeing virtual memory \n");
    task_mode == USER_TASK ? vmm_free_user_space(task->page_dir) : NULL;

    DEBUG("[TASK]: Freeing kernel_stack\n");
    uint32_t phys = virt_to_phys(task->kernel_stack - KERNEL_STACK_SIZE);
    pmm_free(phys);

    DEBUG("[TASK]: Freeing physical page directory\n");
    pmm_free(virt_to_phys((uint32_t)task->page_dir));

    DEBUG("[TASK]: Freeing task\n");
    kfree(task);
}

task_t *get_task_by_name(char *name) { //A helper function not inteded to stay. Doesn't feel like the best solution
    if (name == NULL) return NULL;

    for(int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i] == NULL) {
            continue;
        }
        
        if (strcmp(task_table[i]->name, name) == 0) {
            return task_table[i];
        }
    }
    return NULL;
}
 
task_t *task_get(uint32_t pid) {

    if(pid <= CLERK_COUNT) {
        return task_table[pid];
    }

    if(pid < MAX_TASKS) {    
        for(int i = 0; i < MAX_TASKS; i++) {
            if(task_table[i] != NULL && task_table[i]->pid == pid) {
                return task_table[i];
            }
        }
    }
    return NULL;
}