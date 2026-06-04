#include "blankie.h"
#include "kmalloc.h"
#include "klog.h"
#include "config.h"
#include "task.h"

/*
* Blankie protocol
* Design & Implementation: A.H, 2026
*/

/*
* This file contains the implementation of the blankie protocol. It was designed for a need to reset kernel tasks
* so that their stack would not get corrupted. It also stores their initial state so when kernel_task is awaken again it starts with a clean slate
* from its entry point. 
*/

static blankie_registry_t *b_registry[MAX_LOGS];
static uint8_t req_slot = 0;

int blankie_register(uint32_t pid, uint32_t entry_point, uint32_t stack_top){
    blankie_registry_t *blankie_req = (blankie_registry_t *)kmalloc(sizeof(blankie_registry_t));

    if(req_slot > MAX_LOGS) {
        DEBUG("[BLANKIE][REGISTER]: Not enough blankies for all. Slots filled");
        return STATUS_ERROR;
    }

    if(blankie_req == NULL) {
        DEBUG("[BLANKIE][REGISTER]: Heap allocation failed, aborting\n");
        return STATUS_ERROR;
    }

    blankie_req->pid            = pid;
    blankie_req->entry_point    = entry_point;
    blankie_req->stack_top      = stack_top;

    DEBUG("[BLANKIE][REGISTER]: Current esp 0x%x\n", stack_top);
    DEBUG("[BLANKIE][REGISTER]: Current eip 0x%x\n", stack_top);

    b_registry[req_slot] = blankie_req;
    req_slot++;

    return STATUS_OK;
}

int blankie_activate(uint32_t pid) {
    __asm__ __volatile__("cli");
    task_t *task = task_get(pid);

    if(task == NULL) {
        DEBUG("[BLANKIE][ACTIVATE]: Task not found\n");
        return STATUS_ERROR;
    }

    for(uint8_t i = 0; i < req_slot; i++) {
        if(b_registry[i]->pid == pid) {
            DEBUG("[BLANKIE][ACTIVATE]: Task found %s\n", task->name);
            task->context.eip = b_registry[i]->entry_point;
            task->context.esp = b_registry[i]->stack_top;
            task->context.ebp = b_registry[i]->stack_top;
            task->started     = 0;
            task->state       = TASK_SLEEPING;

            DEBUG("[BLANKIE][ACTIVATE]: Waiting for pit to save us\n");
            DEBUG("[BLANKIE][ACTIVATE]: Current esp 0x%x\n", task->context.esp);
            DEBUG("[BLANKIE][ACTIVATE]: Current eip 0x%x\n", task->context.eip);
            while(1) {
                __asm__ __volatile__("sti; hlt");
            }
        }
    }

    DEBUG("[BLANKIE][ACTIVATE]: No task with the given pid in the registry\n");
    __asm__ __volatile__("sti");
    return STATUS_ERROR;
}