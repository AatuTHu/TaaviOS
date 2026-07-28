#include "task.h"
#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "vmm.h"

/*
 * Task
 * Design & Implementation: A.H, 2026
 */

static task_t *task_table[MAX_TASKS];

task_t *task_create(int reserved_pid, uint32_t entry, const char *name, page_directory_t *page_dir, uint8_t task_mode) {

    if (page_dir == NULL) {
        return NULL;
    }

    int slot = -1;

    if (reserved_pid != -1) {
        slot = reserved_pid;
    } else {
        for (int i = 0; i < MAX_TASKS; i++) {
            if (task_table[i] == NULL) {
                slot = i;
                break;
            }
        }
    }

    if (slot == -1)
        return NULL;

    task_t *task = (task_t *)kmalloc(sizeof(task_t));

    if (task == NULL)
        return NULL;

    if (task_mode == USER_TASK) {
        if (vmm_alloc(page_dir, USER_STACK_TOP, USER_STACK_SIZE, PAGE_USER_RW) == STATUS_ERROR) {
            kfree(task);
            return NULL;
        }
    }

    uint32_t kernel_stack = 0;
    if (vmm_alloc_kstack(&kernel_stack) == STATUS_ERROR) {
        kfree(task);
        return NULL;
    }

    memset(&task->context, 0, sizeof(struct registers));

    task->pid      = slot;
    task->state    = task_mode == USER_TASK ? TASK_READY : TASK_SLEEPING;
    task->page_dir = page_dir;
    task->started  = 0;

    strncpy(task->name, name, sizeof(task->name));
    task->name[TASK_NAME_LENGTH - 1] = '\0';
    task->kernel_stack               = kernel_stack;
    task->context.eip                = entry; // Begining of the task like the main function
    task->context.useresp            = task_mode == USER_TASK
                                           ? USER_STACK_TOP + USER_STACK_SIZE
                                           : task->kernel_stack; // stack pointer?
    task->priority                   = task_mode == USER_TASK ? PRIORITY_NORMAL : PRIORITY_LOW;
    task->context.cs                 = task_mode == USER_TASK ? SEG_USER_CODE : SEG_KERNEL_CODE;
    task->context.ss                 = task_mode == USER_TASK ? SEG_USER_DATA : SEG_KERNEL_DATA;
    task->context.ebp                = task->context.useresp; // stack bottom. Same as top in the begining. No
    task->context.esp                = task->context.useresp;
    task->context.eflags             = EFLAGS_DEFAULT;
    task->task_mode                  = task_mode; // can be usefull later? itwas t: Aatu - 21.6.2026

    task_table[slot]                 = task;

    return task;
}

int task_destroy(task_t *task) {
    if (task == NULL) {
        return STATUS_ERROR;
    }

    DEBUG_TASK("[TASK]: Atempting to destroy given task\n");
    if (task->task_mode == USER_TASK) {
        if (vmm_free_user_space(task->page_dir) == STATUS_ERROR) {
            ERROR("[TASK]: Failed to free virtual page directory\n");
            return STATUS_ERROR;
        }
    }

    if (vmm_free_kstack(task->kernel_stack) == STATUS_ERROR) {
        ERROR("[TASK]: Failed to free kernel stack\n");
        return STATUS_ERROR;
    }

    task_table[task->pid] = NULL;
    kfree(task);
    DEBUG_TASK("[TASK]: Task destroyed\n");
    return STATUS_OK;
}

task_t *task_get(uint32_t pid) {
    if (pid >= MAX_TASKS) {
        return NULL;
    }
    return task_table[pid];
}
