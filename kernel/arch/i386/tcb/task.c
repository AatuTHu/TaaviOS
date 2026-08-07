#include "task.h"
#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "kstring.h"
#include "vmm.h"
#include <stdint.h>

/*
 * Task
 * Design & Implementation: A.H, 2026
 */

task_t *task_table[MAX_TASKS];

task_t *task_create(int reserved_pid, uint32_t entry, uint32_t heap_start, const char *name, page_directory_t *page_dir, uint8_t task_mode) {

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
    task->state    = task_mode == USER_TASK ? TASK_BLOCKED : TASK_SLEEPING;
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
    task->heap_start                 = heap_start;
    task->heap_end                   = heap_start;
    task->context.cs                 = task_mode == USER_TASK ? SEG_USER_CODE : SEG_KERNEL_CODE;
    task->context.ss                 = task_mode == USER_TASK ? SEG_USER_DATA : SEG_KERNEL_DATA;
    task->context.ebp                = task->context.useresp; // stack bottom. Same as top in the begining. No
    task->context.esp                = task->context.useresp;
    task->context.eflags             = EFLAGS_DEFAULT;
    task->task_mode                  = task_mode; // can be usefull later? itwas t: Aatu - 21.6.2026

    task_table[slot]                 = task;

    return task;
}

/**
 * task_sync_kernel_entries_to_all_tasks - when you edit kernel page directory.
 *
 * Description:
 * This function copies kernel directory to every tasks directory.
 * It loops over the MAX_TASKS count to get a tasks with the given index.
 * then it checks if the task is eligible for a sync. After that it memcpies
 * the kernel directory upper indexes to tasks page directory,
 *
 * Context: This was made because I wanted a growable heap and not start with max ram size.
 * And because altering kernel directory is not automatically visible to tasks you have to
 * sync the changes to tasks.
 *
 */
void task_sync_kernel_entries_to_all_tasks() {
    for (int i = 0; i < MAX_TASKS; i++) {
        task_t *task = task_get(i);

        if (task == NULL)
            continue;
        if (task->state == TASK_DEAD)
            continue;
        if (task->page_dir == NULL)
            continue;
        if (task->page_dir == &kernel_page_dir)
            continue;

        memcpy(&(*task->page_dir)[KERNEL_PD_INDEX_START],
               &kernel_page_dir[KERNEL_PD_INDEX_START],
               KERNEL_PD_ENTRIES * sizeof(uint32_t));
    }
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

task_t *task_get(uint32_t index) {
    if (index >= MAX_TASKS) {
        return NULL;
    }
    return task_table[index];
}
