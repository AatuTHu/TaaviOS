#include "sched.h"
#include "klog.h"
#include "config.h"
#include "kstring.h"
#include <stddef.h>

static proc_t *tasks[MAX_PROCESSES];
static int task_count = 0;
static int current_idx = -1;
static uint8_t scheduler_on = 0;

void scheduler_tick(struct registers *r) {

    if(scheduler_on == 0) {
        return;
    }

    if(task_count == 0) {
        //DEBUG("[SCHEDULER] No tasks added\n");
        return;
    } 
    if(current_idx == -1) {
        //DEBUG("[SCHEDULER] No tasks added\n");
        return;
    }

    if(tasks[current_idx]->state == PROCESS_DEAD) {
        //DEBUG("[SCHEDULER] Task is dead\n");
        return;
    }

    //if current task is in blocked state then try and clean another dead task away?
    //cold have a static int of dead tasks counter so that we check if it is higher than 0 and if current task is on block we go to clean dead task


    if(tasks[current_idx]->started) {
        memcpy(&tasks[current_idx]->context, r, sizeof(struct registers));
        tasks[current_idx]->useresp = r->esp;
    }

    tasks[current_idx]->started = 1;
    tasks[current_idx]->state = PROCESS_READY;

    int next_idx = -1;
    for (int i = 1; i <= task_count; i++) {
        int candidate = (current_idx + i) % task_count;
        if (tasks[candidate]->state == PROCESS_READY) {
            next_idx = candidate;
            break;
        }
    }
    
    if (next_idx == -1) return; //this if could put kernel in to a idle state
    
    current_idx = next_idx;
    proc_t *next = tasks[current_idx];
    next->state = PROCESS_RUNNING;
    
    /* Update CPU state for the new process */
    tss_set_kernel_stack(next->kernel_stack);
    vmm_switch(next->page_dir);

     
    memcpy(r, &next->context, sizeof(struct registers));
    r->esp = tasks[current_idx]->useresp;
}

void scheduler_add(proc_t *task) {
    if(task_count >= MAX_PROCESSES) {
        ERROR("[SCHEDULER]: too many tasks added to scheduler\n");
        return;
    }
    tasks[task_count] = task;
    task_count++;

    if(current_idx == -1) {
        current_idx = 0;
    }
}

proc_t *scheduler_get_current() {
    if(current_idx == -1) {
        ERROR("[SCHEDULER]: no tasks added to scheduler yet\n");
        return NULL;
    }
    return tasks[current_idx];
}

void scheduler_remove() { //this is not right
    tasks[current_idx]->state = PROCESS_DEAD;
    tasks[current_idx]->started = 0;
    task_count--;
}

int scheduler_get_task_count() {
    return task_count;
}

void scheduler_init() {
    DEBUG("[SCHEDULER] SCHEDULER INITIALIZED\n");
}

void _set_scheduler_on() {
    scheduler_on = 1;
}