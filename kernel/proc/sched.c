#include "sched.h"
#include "klog.h"
#include "config.h"
#include "kstring.h"
#include "tss.h"
#include "vmm.h"
#include "kernel_idle.h"
#include <stddef.h>

static proc_t *tasks[MAX_PROCESSES];
static int task_count = 0;
static int current_idx = -1;
static uint8_t scheduler_on = 0;

void scheduler_switch_context(struct registers *r, int idx) {
    if(idx < 0 || idx >= MAX_PROCESSES) {
        current_idx = -1;
        DEBUG("[SCHEDULER]: invalid task_idx, cannot make a switch, going to sleep\n");
        vmm_switch((page_directory_t *)kernel_page_dir);
        r->eip  = (uint32_t)kernel_idle;
        r->useresp = 0; 
        r->cs      = SEG_KERNEL_CODE;
        r->ss      = SEG_KERNEL_DATA;
        return;
    }

    current_idx = idx;
    proc_t *next = tasks[current_idx];
    DEBUG("[SCHEDULER]: switching context\n");
    tss_set_kernel_stack(next->kernel_stack);
    vmm_switch(next->page_dir);
    memcpy(r, &next->context, sizeof(struct registers));
    r->esp = tasks[current_idx]->useresp;
    DEBUG("[SCHEDULER]: switching complete\n");
}

int scheduler_find_next() {
   DEBUG("[SCHEDULER]: finding next idx. Current %d\n", current_idx);
   int candidate = -1;
   //Set i to start from current_idx so that we dont run previous task again.
   for(int i = 1; i <= task_count; i++) { 
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->state == PROCESS_READY) {
            candidate = next_idx;
            break;
        }
   }

   if(candidate == -1) {
    DEBUG("[SCHEDULER]: could not find next task\n");
    return candidate;
   }
    DEBUG("[SCHEDULER]: next one found with idx: %d\n", candidate);
    return candidate;
}

//first find next task. Then remove dead task, We can use current_idx here. Then shift all tasks in the array to the left
//Only one task at a time so processor has time for real work.
void _scheduler_task_remove() {

   int next_task_idx = scheduler_find_next();
   if(next_task_idx == -1) return;

   uint32_t next_pid = tasks[next_task_idx]->pid;

   tasks[current_idx] = NULL;
   
   proc_t *new_task_list[task_count];
   memset(new_task_list, 0, task_count * sizeof(proc_t *));

   int j = -1;
   for(int i = 0; i < task_count; i++) { //copy everything except the null.
     if(tasks[i] != NULL) {
         j++;
         new_task_list[j] = tasks[i];
     }
   }

   task_count--;
   memcpy(tasks, new_task_list, task_count * sizeof(proc_t *));

   for(int i = 0; i < task_count; i++) {
    if(tasks[i]->pid == next_pid) {
        current_idx = i;
        break;
    }
   }
   
}

void scheduler_tick(struct registers *r) {

    if(scheduler_on == 0) { //master switch for if for somereason we want turn of the scheduler? Felt cute might delete later
        return;
    }
    
    if(task_count == 0) {
        //DEBUG("[SCHEDULER] No tasks added\n");
        scheduler_switch_context(r, -1);
        return;
    }

    if(current_idx == -1) {
        //DEBUG("[SCHEDULER] No tasks added\n");
        return;
    }
    

    if(tasks[current_idx]->state == PROCESS_DEAD) {
        _scheduler_task_remove();
        return;
    }

    //if current task is in blocked state then try and clean another dead task away?
    //cold have a static int of dead tasks counter so that we check if it is higher than 0 and if current task is on block we go to clean dead task


    if(tasks[current_idx]->started) { //Started true so we don't push shit to context.
        memcpy(&tasks[current_idx]->context, r, sizeof(struct registers));
        tasks[current_idx]->useresp = r->esp;
    }

    tasks[current_idx]->started = 1;
    tasks[current_idx]->state = PROCESS_READY;

    int next_idx = scheduler_find_next();
    
    if (next_idx == -1) { //Nothing to do so go to idle
        scheduler_switch_context(r, -1);
        return;
    }
    
    current_idx = next_idx;
    proc_t *next = tasks[current_idx];

    DEBUG("[SHEDULER]: Now running: %c\n", next->name);

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

void scheduler_kill_task() {
    tasks[current_idx]->state = PROCESS_DEAD;
    tasks[current_idx]->started = 0;
}

int scheduler_get_task_count() {
    int potential_tasks = 0;

    for(int i = 0; i < task_count; i++) {
        if(tasks[i]->state != PROCESS_DEAD) {
            potential_tasks++;
        }
    }

    return potential_tasks;
}

void scheduler_init() {
    DEBUG("[SCHEDULER] SCHEDULER INITIALIZED\n");
}

void _set_scheduler_on() {
    scheduler_on = 1;
}