#include "blankie.h"
#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "sched.h"
#include "task.h"
#include <stdint.h>

/*
 * Blankie protocol
 * Design & Implementation: A.H, 2026
 */

/*
 * This file contains the implementation of the blankie protocol. It was
 * designed for a need to reset kernel tasks so that their stack would not get
 * corrupted. It also stores their initial state so when kernel_task is awaken
 * again it starts with a clean slate from its entry point.
 */

static blankie_registry_t *b_registry[CLERK_COUNT];

int blankie_register(uint32_t pid, uint32_t entry_point, uint32_t stack_top) {
    for (uint8_t i = 0; i < CLERK_COUNT; i++) {
        if (b_registry[i] == NULL) {
            blankie_registry_t *blankie_req =
                (blankie_registry_t *)kmalloc(sizeof(blankie_registry_t));

            if (blankie_req == NULL) {
                DEBUG("[BLANKIE][REGISTER]: Heap allocation failed, aborting\n");
                return STATUS_ERROR;
            }

            blankie_req->pid         = pid;
            blankie_req->entry_point = entry_point;
            blankie_req->stack_top   = stack_top;
            b_registry[i]            = blankie_req;
            // DEBUG("[BLANKIE][REGISTER]: Current esp 0x%x\n", stack_top);
            // DEBUG("[BLANKIE][REGISTER]: Current eip 0x%x\n", stack_top);
            return STATUS_OK;
        }
    }

    return STATUS_ERROR;
}

int blankie_activate(uint32_t pid) {
    for (uint8_t i = 0; i < CLERK_COUNT; i++) {
        if (b_registry[i] != NULL && b_registry[i]->pid == pid) {
            task_t *task      = task_get_by_pid(pid);
            task->context.eip = b_registry[i]->entry_point;
            task->context.esp = b_registry[i]->stack_top;
            task->context.ebp = b_registry[i]->stack_top;
            task->started     = 0;
            task->state       = TASK_SLEEPING;
            scheduler_yield(&task->context);
            break;
        }
    }

    return STATUS_ERROR;
}
