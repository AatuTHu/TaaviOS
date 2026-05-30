#include "sched.h"
#include "klog.h"
#include "config.h"
#include "kstring.h"
#include "tss.h"
#include "vmm.h"
#include "idle_task.h"
#include "print_register.h"
#include <stddef.h>

/* 
* Author: A.H - 20.4.2026
* This code is a pile of sticks. 28.5.2026
* modified continuesly from 20.4 till --- rest of time
*/

static task_t *tasks[MAX_TASKS];
static int task_count = 0;
static int dead_task_count = 0; 
static int kernel_task_count = 0; 
static int user_task_count = 0;
static int current_idx = -1;
static volatile uint8_t scheduler_on = 0;

static void scheduler_spawn_idle_task() {
    
    //DEBUG("[SCHEDULER][SPAWN IDLE TASK]: finding idle task\n");

    task_t *idle_task = tasks[0];

    if(idle_task == NULL && idle_task->task_mode == USER_TASK) {
        DEBUG("[SCHEDULER][SPAWN IDLE TASK]: Did not find idle task\n");
        
        return;
    }

    //DEBUG("[SCHEDULER][SPAWN IDLE TASK]: Idle task found, setting it ready\n");
    idle_task->state = TASK_READY;
    
}

int scheduler_find_next_task() { 
    for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->priority == PRIORITY_HIGH && (tasks[next_idx]->state == TASK_READY || tasks[next_idx]->state == TASK_RUNNING)) {
                return next_idx;
            }
    }

    for(int i = 1; i <= task_count; i++) { 
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->priority == PRIORITY_NORMAL && (tasks[next_idx]->state == TASK_READY || tasks[next_idx]->state == TASK_RUNNING)) {
            return next_idx;      
        }
    }

    for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->priority == PRIORITY_LOW && (tasks[next_idx]->state == TASK_READY || tasks[next_idx]->state == TASK_RUNNING)) {
                return next_idx;
            }
    }
   
    return STATUS_ERROR;
}

/*
*   Scheduler finder func. Give it a state you want to find and id finds the next one based from the current_idx
*   IF candidate is not found it returns as -1
*/
int scheduler_find_first_task_based_on_state(task_state_t state) {
    for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->state == state) {
                return next_idx;
            }
    }
    
    return STATUS_ERROR;
}

/*
* 
*/
void _scheduler_remove_task(struct registers *r) {
    
    //edge case for when there is only one task and it is dead
    if(tasks[current_idx]->state == TASK_DEAD && task_count == 1 && dead_task_count == 1) {
        DEBUG("[SCHEDULER][REMOVE]: Deleting the only task in the scheduler.\n");
        vmm_switch((page_directory_t *)kernel_page_dir);
        task_destroy(tasks[current_idx], KERNEL_TASK);
        tasks[current_idx] = NULL;
        current_idx = -1;
        task_count--;
        dead_task_count--;
        DEBUG("[SCHEDULER][REMOVE]: Remove complete.\n");
        return;
    }

    DEBUG("[SCHEDULER][REMOVE]: Searching for a dead task\n");
    int delete_candidate = scheduler_find_first_task_based_on_state(TASK_DEAD);
   
    if(delete_candidate == -1) {
        DEBUG("[SCHEDULER][REMOVE]: No deletable task found.\n");
        
        return;
    }
    
    DEBUG("[SCHEDULER][REMOVE]: Deleting task at current idx %d with name: %s\n", delete_candidate, tasks[delete_candidate]->name);
    vmm_switch(&kernel_page_dir);
    task_destroy(tasks[delete_candidate], USER_TASK);
    tasks[delete_candidate]->task_mode == USER_TASK ? user_task_count-- : kernel_task_count--;
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
        DEBUG("[SCHEDULER][REMOVE]: Deleted current idx, trying to switch to another task\n");
        current_idx = 0;
    }
}


// The core switching logic, shared by both
static void scheduler_switch(struct registers *r) {
    task_t *current = scheduler_get_current_task();

    if(current != NULL && current->started) {
        //DEBUG("[SCHEDULER][SWITCH]: Saving: %s\n", current->name);
        memcpy(&current->context, r, sizeof(struct registers));

        if(current->state == TASK_RUNNING) {
            current->state = TASK_READY;
        }
    }

    
    if(scheduler_has_runnable_task() == 0) {
        DEBUG("[SCHEDULER][SWITCH]: There are no runnable tasks. Checking if clerks have servicing.\n");
        scheduler_spawn_idle_task();
    }

    int next_idx = scheduler_find_next_task();

    if(next_idx == -1 || next_idx >= MAX_TASKS) {
        ERROR("[SCHEDULER][SWITCH]: Panic, no tasks available.\n");
        __asm__ __volatile__("sti; hlt");
    }

    task_t *next = tasks[next_idx];
    next->state = TASK_RUNNING;
    next->started = 1;

    if(next_idx != current_idx) {
        //DEBUG("[SCHEDULER][SWITCH]: Running: %s\n", next->name);
        current_idx = next_idx;
        vmm_switch(next->page_dir);
        tss_set_kernel_stack(next->kernel_stack);
        memcpy(r, &next->context, sizeof(struct registers));
    }
}

void scheduler_yield(struct registers *r) { 
    __asm__ __volatile__("cli");
    if(scheduler_on == 0) return;
    scheduler_switch(r);
}

void scheduler_tick(struct registers *r) {
    __asm__ __volatile__("cli");
    if(current_idx == -1 || task_count == 0 || scheduler_on == 0) {
        __asm__ __volatile__("sti");
        return;
    }

    scheduler_switch(r);

    __asm__ __volatile__("sti");
}

/*
* HELPER FUNCTIONS
*/

int scheduler_set_current_task(uint32_t pid) {
     for(int i = 1; i <= task_count; i++) { 
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->pid == pid) {
            current_idx = next_idx;
        }
    }
}

task_t *scheduler_get_current_task() {
    if(current_idx == -1) {
        ERROR("[SCHEDULER]: no tasks added to scheduler yet\n");
        return NULL;
    }
    return tasks[current_idx];
}

void scheduler_set_task_state(task_state_t state) {
    task_t *current = tasks[current_idx];

    if(current->state == state) {
        DEBUG("[SCHEDULER][STATE_SETTER]: No need to set tasks state as it already is the state\n");
        return;
    }

    switch (state)
    {
    case TASK_SLEEPING:
        //DEBUG("[SCHEDULER][STATE_SETTER]: Setting task %s sleeping\n", current->name);
        current->state = TASK_SLEEPING;
        break;
    case TASK_READY:
        //DEBUG("[SCHEDULER][STATE_SETTER]: Setting task %s ready\n", current->name);
        if(current->state == TASK_DEAD && dead_task_count > 0) {
            dead_task_count--;
        }
        current->state = TASK_READY;
        break;
    case TASK_BLOCKED:
        //DEBUG("[SCHEDULER][STATE_SETTER]: Blocking task: %s\n", current->name); 
        if(current->state != TASK_DEAD) {
            current->state = TASK_BLOCKED;
        }
        break;
    case TASK_DEAD:
        //DEBUG("[SCHEDULER][STATE_SETTER]: killing task: %s\n", current->name);    
        current->state = TASK_DEAD;
        dead_task_count++;
        break;
    
    default:
        break;
    }

}

int scheduler_get_task_count() { 
    return task_count;
}

int scheduler_has_runnable_task() {

    for (int i = 0; i < task_count; i++) {
        if (tasks[i] && tasks[i]->state == TASK_READY) {
            return 1;
        }
    }
    return 0;
}

void scheduler_wake_task(uint32_t pid) {
    for(int i = 0; i < task_count; i++) {
        if (tasks[i] && tasks[i]->pid == pid) {
            //DEBUG("[SCHEDULER]: Waking task %s\n", tasks[i]->name);
            tasks[i]->state = TASK_READY;
            break;
        }
    }
}


int scheduler_does_exist(int pid) {
    for(int i = 0; i < task_count; i++) {
        if(tasks[i]->pid == pid) {
            //DEBUG("[SCHEDULER][DOES_EXIST]: Task exists\n");
            return STATUS_ERROR;
        }
    }
    //DEBUG("[SCHEDULER][DOES_EXIST]: Task does not exists\n");
    return STATUS_OK;
}

void scheduler_add(task_t *task) {
    
    if(task_count >= MAX_TASKS) {
        ERROR("[SCHEDULER][ADD]: Too many tasks added to scheduler\n");
        return;
    }
    
    if(scheduler_does_exist(task->pid) == STATUS_ERROR) {
       // DEBUG("[SCHEDULER][ADD]: Aborting\n");
        return;
    }
    
    tasks[task_count] = task;
    
    task_count++;
    if(current_idx == -1) {
        current_idx = 0;
    }
}

int scheduler_get_idx_off_pid(uint32_t pid) {
     for(int i = 1; i <= task_count; i++) { 
        int pids_idx = (current_idx + i) % task_count;
        if (tasks[pids_idx]->pid == pid) {
            return pids_idx;      
        }
    }
}

void _set_scheduler_on() {
    scheduler_on = 1;
}

void scheduler_init() {
    DEBUG("[SCHEDULER] SCHEDULER INITIALIZED\n");
}
