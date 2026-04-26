#include "sched.h"
#include "klog.h"
#include "config.h"
#include "kstring.h"
#include "tss.h"
#include "vmm.h"
#include "kernel_idle.h"
#include <stddef.h>


/* 
Author: A.H - 20.4.2026
*/

static proc_t *tasks[MAX_PROCESSES];
static int task_count = 0;
static int dead_task_count = 0;
static int current_idx = -1;
volatile static uint8_t scheduler_on = 0;


/*
*   If scheduler is not on go to sleep. This is because in kernelmain when we jump to usermode we jump with a task that is added to scheduler.
*   if other tasks are added to scheduler then this func wil find and run them when the task makes a sys_exit call.
*/
int scheduler_switch_context(struct registers *r, int idx) { 

    if(scheduler_on == 0) { 
        r->eip  = (uint32_t)kernel_idle;
        r->useresp = 0; 
        r->cs      = SEG_KERNEL_CODE;
        r->ss      = SEG_KERNEL_DATA;
        return 0;
    }

    if(idx < 0 || idx >= MAX_PROCESSES) {
        //DEBUG("[SCHEDULER]: invalid idx, cannot make a switch\n");
        return -1;
    }

    current_idx = idx;
    proc_t *next = tasks[current_idx];

    //DEBUG("[SCHEDULER]: switching context\n");

    if(tasks[current_idx]->started) {
        memcpy(&tasks[current_idx]->context, r, sizeof(struct registers));
    }

    vmm_switch(next->page_dir);
    tss_set_kernel_stack(next->kernel_stack);

    next->state = PROCESS_READY;

    memcpy(r, &next->context, sizeof(struct registers));
    //DEBUG("[SCHEDULER]: switching complete\n");
}

int scheduler_find_next_task() {
    for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->priority == PRIORITY_HIGH) {
                return next_idx;
            }
    }

    for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->priority == PRIORITY_NORMAL) {
                return next_idx;
            }
    }
    
    for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->priority == PRIORITY_LOW) {
                return next_idx;
            }
    }
    return -1;
}

/*
*   Scheduler finder func. Give it a state you want to find and id finds the next one based from the current_idx
*   IF candidate is not found it returns as -1
*/
int scheduler_find_first_task_based_on_state(process_state_t state) {
    int candidate = -1;
    for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->state == state) {
                candidate = next_idx;
                break;
            }
    }
    return candidate;
}

/*
*   First we try and find next task from the list to run. Beacouse once we delete task we cannot know for certain what was the next one.
*   If we don't find tasks that are ready to run we are going to run DEAD task next, so that scheduler comes here again. [THIS SHOULD BE CHANGED TO BLOCKED?]
*   ---
*   Second we are going to find a task that is in a dead state and we delete it.
*   Thirdy we shift the old list on left so that there are no nulls
*   Fourth we find the idx which corresponds to our next_pid
*/
void _scheduler_remove_task() {

   int pid_after_deletion = -1;
   int next_task_idx = scheduler_find_first_task_based_on_state(PROCESS_READY);
   DEBUG("[SCHEDULER]: next idx: %d\n", next_task_idx);
   
   if(next_task_idx == -1) { 
       DEBUG("[SCHEDULER]: No tasks with a ready state found. Finding dead task to run next\n");
       int deletable_idx = scheduler_find_first_task_based_on_state(PROCESS_DEAD);

        if(deletable_idx == -1) {
            DEBUG("[SCHEDULER]: Nothing to delete, returning\n");
            return;
        }
       pid_after_deletion = tasks[deletable_idx]->pid;
    }


    //edge case for when there is only one task and it is dead
    if(tasks[pid_after_deletion]->state == PROCESS_DEAD && task_count == 1 && dead_task_count == 1) {
        DEBUG("[SCHEDULER]: Deleting the only task in the scheduler.\n");
        vmm_switch((page_directory_t *)kernel_page_dir);
        process_destroy(tasks[pid_after_deletion]);
        tasks[pid_after_deletion] = NULL;
        current_idx = -1;
        task_count--;
        dead_task_count--;
        DEBUG("[SCHEDULER]: Remove complete.\n");
        DEBUG("[SCHEDULER]: Going to sleep\n");
        return;
    }

   int delete_candidate = -1;
   for(int i = 1; i <= task_count; i++) { 
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->state == PROCESS_DEAD && pid_after_deletion != tasks[next_idx]->pid) {
            delete_candidate = next_idx;
            break;
        }
   }
   
   if(delete_candidate == -1) {
        DEBUG("[SCHEDULER]: No deletable task found.\n");
        return;
   }
    
    DEBUG("[SCHEDULER]: Deleting task at current idx %d with name: %s\n", delete_candidate, tasks[delete_candidate]->name);
    DEBUG("[SCHEDULER]: Switching to kernel_page_dir\n");
    vmm_switch((page_directory_t *)kernel_page_dir);
    process_destroy(tasks[delete_candidate]);
    tasks[delete_candidate] = NULL;

   proc_t *new_task_list[task_count];
   memset(new_task_list, 0, task_count * sizeof(proc_t *));
   
   DEBUG("[SCHEDULER]: Shifting rest of the array to the left\n");
   int j = -1;
   for(int i = 0; i < task_count; i++) { //copy everything except the null.
        if(tasks[i] != NULL) {
            j++;
            new_task_list[j] = tasks[i];
        }
    }

    task_count--;
    dead_task_count--;
    memcpy(tasks, new_task_list, task_count * sizeof(proc_t *));

    DEBUG("[SCHEDULER]: Finding new task\n");
    for(int i = 0; i < task_count; i++) {
        if(tasks[i]->pid == pid_after_deletion) {
            current_idx = i;
            break;
        }
    }
    DEBUG("[SCHEDULER]: Task found! Task name: %s, idx: %d\n", tasks[current_idx]->name, current_idx);
}


/*
* First we make the basic checks so that we know if there is a reason to switch task on this tick.
* scheduler_on is a master switch if that I could see as a syscall that init task makes when everything is ready.
* Second Then if tasks state is blocked or dead we can use that timespace to delete task.
* Third Then basic context-switch. If task is started we save registers to tasks context.
* Fourth see if next idx is the same as now. if it is then no need to switch context.
*/
void scheduler_tick(struct registers *r) {

    if(scheduler_on == 0) { //master switch for if for somereason we want turn of the scheduler? Felt cute might delete later
        return;
    }
    
    if(current_idx == -1) {
        return;
    }

    if(task_count == 0) {
        return;
    }
    
    if(tasks[current_idx]->state == PROCESS_DEAD) {
        _scheduler_remove_task();
        return;
    }

    if(tasks[current_idx]->started && tasks[current_idx]->state != PROCESS_BLOCKED) {
        memcpy(&tasks[current_idx]->context, r, sizeof(struct registers));
    }

    tasks[current_idx]->started = 1;
    tasks[current_idx]->state = PROCESS_READY;
    
    int next_idx = scheduler_find_next_task();
    
    if (next_idx == -1) { 
        return;
    }
    
    if(next_idx != current_idx) {
        current_idx = next_idx;
        proc_t *next = tasks[current_idx];
        vmm_switch(next->page_dir);
        tss_set_kernel_stack(next->kernel_stack);
        
        next->state = PROCESS_RUNNING;
        memcpy(r, &next->context, sizeof(struct registers));
    }
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

proc_t *scheduler_get_current_task() {
    if(current_idx == -1) {
        ERROR("[SCHEDULER]: no tasks added to scheduler yet\n");
        return NULL;
    }
    return tasks[current_idx];
}

void scheduler_kill_task() {
    DEBUG("[SCHEDULER]: killing current task: %s\n", tasks[current_idx]->name);
    tasks[current_idx]->state = PROCESS_DEAD;
    tasks[current_idx]->started = 0;
    dead_task_count++;
}

void scheduler_block_task(struct registers *r) {
    //memcpy(&tasks[current_idx]->context, r, sizeof(struct registers));
    tasks[current_idx]->state = PROCESS_BLOCKED;   
}

void scheduler_set_task_ready() {
    if(tasks[current_idx]->state == PROCESS_DEAD) {
        dead_task_count--;
    }
    tasks[current_idx]->state = PROCESS_READY;
}

//We only want to know the task that are no dead
int scheduler_get_task_count() { 
    int potential_tasks = 0;

    for(int i = 1; i < task_count; i++) {
        if(tasks[i]->state != PROCESS_DEAD) {
            potential_tasks++;
        }
    }

    return potential_tasks;
}

void scheduler_wake_task(int pid) {
    for(int i = 0; i < task_count; i++) {
        if(tasks[i]->pid == pid) {
            tasks[i]->state = PROCESS_READY;
            break;
        }
    }
}

void _set_scheduler_on() {
    scheduler_on = 1;
}


void scheduler_init() {
    DEBUG("[SCHEDULER] SCHEDULER INITIALIZED\n");
}
