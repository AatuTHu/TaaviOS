#include "blankie.h"
#include "config.h"
#include "klog.h"
#include "kmalloc.h"
#include "task.h"

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
    blankie_registry_t *blankie_req =
        (blankie_registry_t *)kmalloc(sizeof(blankie_registry_t));

    if (blankie_req == NULL) {
        DEBUG("[BLANKIE][REGISTER]: Heap allocation failed, aborting\n");
        return STATUS_ERROR;
    }

    blankie_req->pid         = pid;
    blankie_req->entry_point = entry_point;
    blankie_req->stack_top   = stack_top;

    // DEBUG("[BLANKIE][REGISTER]: Current esp 0x%x\n", stack_top);
    // DEBUG("[BLANKIE][REGISTER]: Current eip 0x%x\n", stack_top);

    // as only kernel clerks get this protocol we can use their pid as index. to
    // achieve 0(1) ratio but as idle is pid 0 we have to do some voodoo to pid
    // to get the ratio

    b_registry[pid - 1] = blankie_req;

    return STATUS_OK;
}

int blankie_activate(uint32_t pid) {
    task_t *task = task_get(pid);

    if (task == NULL) {
        return STATUS_ERROR;
    }

    task->context.eip = b_registry[pid - 1]->entry_point;
    task->context.esp = b_registry[pid - 1]->stack_top;
    task->context.ebp = b_registry[pid - 1]->stack_top;
    task->started     = 0;
    task->state       = TASK_SLEEPING;

    DEBUG("[BLANKIE][ACTIVATE]: Waiting for pit to save %s\n", task->name);
    while (1) {}

    return STATUS_OK;
}