#include "sched.h"
#include "klog.h"
#include "config.h"
#include "kstring.h"
#include "tss.h"
#include "vmm.h"
#include "kernel_idle.h"
#include "print_register.h"
#include <stddef.h>

/* 
* Author: A.H - 20.4.2026
* modified continuesly from 20.4 till ---
*/

static proc_t *tasks[MAX_PROCESSES];
static int task_count = 0;
static int dead_task_count = 0;
static int current_idx = -1;
volatile static uint8_t scheduler_on = 0;

int scheduler_find_next_task() {
    for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->priority == PRIORITY_HIGH) {
                return next_idx;
            }
    }
    
    if(dead_task_count > 0) {
        for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->state == PROCESS_BLOCKED || tasks[next_idx]->state == PROCESS_DEAD) {
                return next_idx;
            }
        }
    }

    for(int i = 1; i <= task_count; i++) { 
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->priority == PRIORITY_NORMAL && tasks[next_idx]->state == PROCESS_READY) {
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
   
   if(next_task_idx == -1) {
        next_task_idx = scheduler_find_first_task_based_on_state(PROCESS_BLOCKED);
   }

   if(next_task_idx == -1) {
        next_task_idx = scheduler_find_first_task_based_on_state(PROCESS_DEAD);
   }


   if(next_task_idx == -1) return;
   pid_after_deletion = tasks[next_task_idx]->pid;

    //edge case for when there is only one task and it is dead
    if(tasks[next_task_idx]->state == PROCESS_DEAD && task_count == 1 && dead_task_count == 1) {
        DEBUG("[SCHEDULER][REMOVE]: Deleting the only task in the scheduler.\n");
        vmm_switch((page_directory_t *)kernel_page_dir);
        process_destroy(tasks[next_task_idx]);
        tasks[next_task_idx] = NULL;
        current_idx = -1;
        task_count--;
        dead_task_count--;
        DEBUG("[SCHEDULER][REMOVE]: Remove complete.\n");
        return;
    }

   int delete_candidate = -1;
   for(int i = 1; i <= task_count; i++) { 
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->state == PROCESS_DEAD) {
            delete_candidate = next_idx;
            break;
        }
   }
   
   if(delete_candidate == -1) {
        DEBUG("[SCHEDULER][REMOVE]: No deletable task found.\n");
        return;
   }
    
    DEBUG("[SCHEDULER][REMOVE]: Deleting task at current idx %d with name: %s\n", delete_candidate, tasks[delete_candidate]->name);
    vmm_switch(&kernel_page_dir);
    process_destroy(tasks[delete_candidate]);
    tasks[delete_candidate] = NULL;
   
   DEBUG("[SCHEDULER][REMOVE]: Shifting rest of the array to the left\n");
    for (int i = delete_candidate; i < task_count - 1; i++) {
        tasks[i] = tasks[i + 1];
    }
    tasks[task_count - 1] = NULL;
    task_count--;
    dead_task_count--;

    if(delete_candidate < current_idx) {
        current_idx--;
        DEBUG("[SCHEDULER][REMOVE]: current idx pointing to %s\n", tasks[current_idx]->name);
    } else if(delete_candidate == current_idx) {
        current_idx = -1;
    }
}

/*
*   If scheduler is not on go to sleep. This is because in kernelmain when we jump to usermode we jump with a task that is added to scheduler.
*   if other tasks are added to scheduler then this func wil find and run them when the task makes a sys_exit call.
*/
void scheduler_switch_context(struct registers *r, int idx) { 

    if(scheduler_on == 0 || idx < 0 || idx >= MAX_PROCESSES) return;

    proc_t *current = tasks[current_idx];
    if(current != NULL) {
        //DEBUG("[SCHEDULER][CONTEXT_SWITCH]: saving: %s\n", current->name);
        if(current->started) {
            memcpy(&current->context, r, sizeof(struct registers));
        }
        if(current->state == PROCESS_RUNNING) {
            current->state = PROCESS_READY;
        }
    }

    proc_t *next = tasks[idx];

    if(current_idx != idx) {
        DEBUG("[SCHEDULER][CONTEXT_SWITCH]: now running: %s\n", next->name);
        current_idx = idx;
        vmm_switch(next->page_dir);
        next->started = 1;
        tss_set_kernel_stack(next->kernel_stack);
        memcpy(r, &next->context, sizeof(struct registers));
        if(next->state == PROCESS_READY) {
            next->state = PROCESS_RUNNING;
        }
    }
}


/*
* First we make the basic checks so that we know if there is a reason to switch task on this tick.
* scheduler_on is a master switch if that I could see as a syscall that init task makes when everything is ready.
* Second Then if tasks state is blocked or dead we can use that timespace to delete task.
* Third Then basic context-switch. If task is started we save registers to tasks context.
* Fourth see if next idx is the same as now. if it is then no need to switch context.
*/
void scheduler_tick(struct registers *r) {
    
    if(current_idx == -1 || task_count == 0 || scheduler_on == 0) return;

    proc_t *current = scheduler_get_current_task();
    if(current == NULL) return;
    
    if(current->started) {
       // DEBUG("[SCHEDULER][TICK]: saving: %s\n", current->name);
        memcpy(&current->context, r, sizeof(struct registers));
        if(current->state == PROCESS_RUNNING) {
            current->state = PROCESS_READY;
        }
    }
    current->started = 1;
    
    
    if((current->state == PROCESS_DEAD || current->state == PROCESS_BLOCKED) && dead_task_count > 0) {
        DEBUG("[SCHEDULER][TICK]: Going for clean up with %s\n", current->name);
        DEBUG("[SCHEDULER][TICK]: dead task count %d\n", dead_task_count);
        _scheduler_remove_task();
    }
    
    int next_idx = scheduler_find_next_task();
    if (next_idx == -1) return;
    
    proc_t *next = tasks[next_idx];
    
    if(next->state == PROCESS_BLOCKED || next->state == PROCESS_DEAD) return;
    
    next->state = PROCESS_RUNNING;
    if(next_idx != current_idx) {
        DEBUG("[SCHEDULER][TICK]: now running: %s\n", next->name);
        current_idx = next_idx;
        vmm_switch(next->page_dir);
        tss_set_kernel_stack(next->kernel_stack);
        memcpy(r, &next->context, sizeof(struct registers));
    }
}




/*
* HELPER FUNCTIONS
*/
proc_t *scheduler_get_current_task() {
    if(current_idx == -1) {
        ERROR("[SCHEDULER]: no tasks added to scheduler yet\n");
        return NULL;
    }
    return tasks[current_idx];
}

void scheduler_kill_task() {
    DEBUG("[SCHEDULER]: killing current task: %s\n", tasks[current_idx]->name);
    if(tasks[current_idx]->state != PROCESS_DEAD) {
        tasks[current_idx]->state = PROCESS_DEAD;
        dead_task_count++;
    }
}

void scheduler_block_task(struct registers *r) {
    DEBUG("[SCHEDULER][block_task]: blocking task: %s\n", tasks[current_idx]->name);
    if(tasks[current_idx]->state != PROCESS_BLOCKED) {
        tasks[current_idx]->state = PROCESS_BLOCKED;   
    }
}

void scheduler_set_task_ready() {
    if(tasks[current_idx]->state == PROCESS_DEAD) {
        dead_task_count--;
    }
    DEBUG("[SCHEDULER][set_task_ready]: setting task %s ready\n", tasks[current_idx]->name);
    tasks[current_idx]->state = PROCESS_READY;
}

int scheduler_get_task_count() { 
    return task_count;
}

int scheduler_has_runnable_task() {
    for (int i = 0; i < task_count; i++) {
        if (tasks[i] && tasks[i]->state == PROCESS_READY && tasks[i]->pid != tasks[current_idx]->pid) {
            return 1;
        }
    }
    return 0;
}

void scheduler_wake_task(int pid) {
    for(int i = 0; i < task_count; i++) {
        if (tasks[i] && tasks[i]->pid == pid) {
            DEBUG("[SCHEDULER]: waking task %s\n", tasks[i]->name);
            tasks[i]->state = PROCESS_READY;
            break;
        }
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

int scheduler_does_exist(int pid) {
    for(int i = 0; i < task_count; i++) {
        if(tasks[i]->pid == pid) {
            DEBUG("[SCHEDULER]: Task exists\n");
            return 1;
        }
    }
    DEBUG("[SCHEDULER]: task does not exists\n");
    return 0;
}

void _set_scheduler_on() {
    scheduler_on = 1;
}


void scheduler_init() {
    //proc_t *idle_task = process_create((uint32_t)kernel_idle, "idle", &kernel_page_dir, KERNEL_PROCESS);
    //scheduler_add(idle_task);
    DEBUG("[SCHEDULER] SCHEDULER INITIALIZED\n");
}
