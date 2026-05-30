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
            if (tasks[next_idx]->priority == PRIORITY_HIGH && tasks[next_idx]->state != TASK_DEAD) {
                return next_idx;
            }
    }

    for(int i = 1; i <= task_count; i++) { 
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->priority == PRIORITY_NORMAL && tasks[next_idx]->state == TASK_READY) {
            return next_idx;      
        }
    }

    for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->priority == PRIORITY_LOW && (tasks[next_idx]->state == TASK_READY || tasks[next_idx]->state == TASK_RUNNING)) {
                return next_idx;
            }
    }

    
    return -1;
}

/*
*   Scheduler finder func. Give it a state you want to find and id finds the next one based from the current_idx
*   IF candidate is not found it returns as -1
*/
int scheduler_find_first_task_based_on_state(task_state_t state) {
    
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

/*
*   If scheduler is not on go to sleep. This is because in kernel main when we jump to usermode we jump with a task that is added to scheduler.
*   if other tasks are added to scheduler then this func wil find and run them when the task makes a sys_exit call.
*/
void scheduler_switch_context(struct registers *r, int idx) { 
    __asm__ __volatile__("cli");

    if(scheduler_on == 0) {
        DEBUG("[SCHEDULER][CONTEXT_SWITCH]: scheudler_on: %d\n", scheduler_on);
    }

    
    task_t *current = tasks[current_idx];
    if(current != NULL) {
        //DEBUG("[SCHEDULER][CONTEXT_SWITCH]: saving: %s\n", current->name);
        if(current->started) {
            memcpy(&current->context, r, sizeof(struct registers));
        }
        if(current->state == TASK_RUNNING) {
            current->state = TASK_READY;
        }
    }
    
    if(scheduler_has_runnable_task() == 0) {
        //DEBUG("[SCHEDULER][CONTEXT_SWITCH]: There is no tasks to switch to spawning idle task\n");
        scheduler_spawn_idle_task();
    }

    if(idx < 0 || idx >= MAX_TASKS) {
        DEBUG("[SCHEDULER][CONTEXT_SWITCH]: Cannot switch to idx: %d, trying to find new one\n", idx);
        

        idx = scheduler_find_next_task();

        if(idx == -1) {
            DEBUG("[SCHEDULER][CONTEXT_SWITCH]: Not even idle task can save this. Shutting down\n");
            __asm__ __volatile__("sti; hlt");
            return;
        }
    }
    
    task_t *next = tasks[idx];

    if((current->state == TASK_DEAD || current->state == TASK_BLOCKED)) {
        idx = scheduler_find_first_task_based_on_state(TASK_READY);
        next = tasks[idx];
    }

    if(current_idx != idx) {
        DEBUG("[SCHEDULER][CONTEXT_SWITCH]: now running: %s\n", next->name);
        current_idx = idx;
        vmm_switch(next->page_dir);
        next->started = 1;
        tss_set_kernel_stack(next->kernel_stack);
        memcpy(r, &next->context, sizeof(struct registers));
        if(next->state == TASK_READY) {
            next->state = TASK_RUNNING;
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
    __asm__ __volatile__("cli");
    if(current_idx == -1 || task_count == 0 || scheduler_on == 0) {
        DEBUG("[SCHEDULER][TICK]: cidx: %d, count: %d, mode: %d\n", current_idx,task_count,scheduler_on);
        __asm__ __volatile__("sti");
        return;
    }

    task_t *current = scheduler_get_current_task();
    if(current == NULL) {
        DEBUG("[SCHEDULER][TICK]: Did not find current task \n");
        __asm__ __volatile__("sti");
        return;
    }
    
    if(current->started) {
        memcpy(&current->context, r, sizeof(struct registers));
        if(current->state == TASK_RUNNING) {
            current->state = TASK_READY;
        }
    }
    current->started = 1;
    
    
    if((current->state == TASK_DEAD || current->state == TASK_BLOCKED) && dead_task_count > 0) {
        DEBUG("[SCHEDULER][TICK]: Going for clean up with %s\n", current->name);
        DEBUG("[SCHEDULER][TICK]: Dead task count %d\n", dead_task_count);
        _scheduler_remove_task(r);
        return;
    }
    
    if(scheduler_has_runnable_task() == 0) {
        DEBUG("[SCHEDULER][TICK]: There is no tasks to switch to spawning idle task\n");
        scheduler_spawn_idle_task();
    }
    //DEBUG("[SCHEDULER][TICK]: finding next task to run\n");
    int next_idx = scheduler_find_next_task();
    //DEBUG("[SCHEDULER][TICK]: new idx: %d\n", next_idx);
    
    if(next_idx == -1) {
        DEBUG("[SCHEDULER][TICK: Not even idle task can save this. Shutting down\n");
        __asm__ __volatile__("sti; hlt");
        return;
    }

    task_t *next = tasks[next_idx];
    
    if(next == NULL) {
        DEBUG("[SCHEDULER][TICK]: Something went horribly wrong\n");
        __asm__ __volatile__("sti");
        return;
    }

    next->state = TASK_RUNNING;
    
    if(next_idx != current_idx) {
        current_idx = next_idx;
       // DEBUG("[SCHEDULER][TICK]: Now running: %s\n", next->name);

        if(next->task_mode == USER_TASK) {
            vmm_switch(next->page_dir);
        }

        tss_set_kernel_stack(next->kernel_stack);
        memcpy(r, &next->context, sizeof(struct registers));
    }
    //DEBUG("[SCHEDULER][TICK]: off we go\n");
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
        //DEBUG("[SCHEDULER][STATE_SETTER]: No need to set tasks state as it already is the state\n");
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
