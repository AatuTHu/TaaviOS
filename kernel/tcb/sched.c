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
* modified continuesly from 20.4 till ---
*/

static task_t *tasks[MAX_TASKS];
static int task_count = 0;
static int dead_task_count = 0;
static int current_idx = -1;
volatile static uint8_t scheduler_on = 0;

int scheduler_find_next_task() {
    __asm__ __volatile__("cli");
    for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->priority == PRIORITY_HIGH) {
                return next_idx;
            }
    }

    if(dead_task_count > 0) {
        DEBUG("[SCHEDULER][SEARCH]: Searching for someone to clean up tasks\n");
        for(int i = 1; i <= task_count; i++) { 
            int next_idx = (current_idx + i) % task_count;
            if (tasks[next_idx]->state == TASK_BLOCKED || tasks[next_idx]->state == TASK_DEAD) {
                DEBUG("[SCHEDULER][SEARCH]: Cleaner found %s\n", tasks[next_idx]->name);
                return next_idx;
            }
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

    __asm__ __volatile__("sti");
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
*   First we try and find next task from the list to run. Beacouse once we delete task we cannot know for certain what was the next one.
*   If we don't find tasks that are ready to run we are going to run DEAD task next, so that scheduler comes here again. [THIS SHOULD BE CHANGED TO BLOCKED?]
*   ---
*   Second we are going to find a task that is in a dead state and we delete it.
*   Thirdy we shift the old list on left so that there are no nulls
*   Fourth we find the idx which corresponds to our next_pid
*/
void _scheduler_remove_task() {
    
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
        DEBUG("[SCHEDULER][REMOVE]: Deleted current idx\n");
        current_idx = -1;
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
    
    if(idx < 0 || idx >= MAX_TASKS) {
        DEBUG("[SCHEDULER][CONTEXT_SWITCH]: CANNOT SWITCH CONTEXT\n");
        DEBUG("[SCHEDULER][CONTEXT_SWITCH]: trying to switch to idx: %d\n", idx);
        return;
    }
    
    task_t *next = tasks[idx];

    if(current_idx != idx) {
        //DEBUG("[SCHEDULER][CONTEXT_SWITCH]: now running: %s\n", next->name);
        current_idx = idx;
        vmm_switch(next->page_dir);
        next->started = 1;
        tss_set_kernel_stack(next->kernel_stack);
        memcpy(r, &next->context, sizeof(struct registers));
        if(next->state == TASK_READY) {
            next->state = TASK_RUNNING;
        }
    }
    __asm__ __volatile__("sti");
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
    if(current_idx == -1 || task_count == 0 || scheduler_on == 0) return;

    task_t *current = scheduler_get_current_task();
    if(current == NULL) return;
    
    if(current->started) {
        //DEBUG("[SCHEDULER][TICK]: saving: %s\n", current->name);
        memcpy(&current->context, r, sizeof(struct registers));
        if(current->state == TASK_RUNNING) {
            current->state = TASK_READY;
        }

        //LEGACY CODE FOR NOW KEEPING IT FOR ISNPIRATION
        /*if(current->pid == 0 && task_count == 1 && dead_task_count == 0) {
            DEBUG("[SCHEDULER][TICK]: idle is the only task remaining. Shutting down\n");
            scheduler_kill_task();
            _scheduler_remove_task();
            __asm__ __volatile__("sti; hlt");
        }*/
    }
    current->started = 1;
    
    
    if((current->state == TASK_DEAD || current->state == TASK_BLOCKED) && dead_task_count > 0) {
        DEBUG("[SCHEDULER][TICK]: Going for clean up with %s\n", current->name);
        DEBUG("[SCHEDULER][TICK]: Dead task count %d\n", dead_task_count);
        _scheduler_remove_task();
    }
    
    //DEBUG("[SCHEDULER][TICK]: finding next task to run\n");
    int next_idx = scheduler_find_next_task();

    if (next_idx == -1) {
        DEBUG("[SCHEDULER][TICK]: No new task found, fetching idle task to run.\n");
        task_t *idle_task = get_task_by_name("idle");
        if(idle_task != NULL) {
            DEBUG("[SCHEDULER][TICK]: Waking idle task\n");
            idle_task->state = TASK_READY;
            next_idx = scheduler_get_idx_off_pid(idle_task->pid);
        }
       // return;
    }
    
    task_t *next = tasks[next_idx];
    
    if(next == NULL || next->state == TASK_BLOCKED || next->state == TASK_DEAD) return;

    next->state = TASK_RUNNING;
    
    if(next_idx != current_idx) {
        //DEBUG("[SCHEDULER][TICK]: Now running: %s\n", next->name);
        current_idx = next_idx;
        vmm_switch(next->page_dir);
        tss_set_kernel_stack(next->kernel_stack);
        memcpy(r, &next->context, sizeof(struct registers));
    }
    __asm__ __volatile__("sti");
}




/*
* HELPER FUNCTIONS
*/
task_t *scheduler_get_current_task() {
    if(current_idx == -1) {
        ERROR("[SCHEDULER]: no tasks added to scheduler yet\n");
        return NULL;
    }
    return tasks[current_idx];
}

void scheduler_kill_task() {
    DEBUG("[SCHEDULER]: killing current task: %s\n", tasks[current_idx]->name);
    if(tasks[current_idx]->state != TASK_DEAD) {
        tasks[current_idx]->state = TASK_DEAD;
        dead_task_count++;
    }
}

void scheduler_block_task() {
    if(tasks[current_idx]->state != TASK_BLOCKED && tasks[current_idx]->state != TASK_DEAD) {
        tasks[current_idx]->state = TASK_BLOCKED;
    }
}

void scheduler_set_task_sleeping() {
    //DEBUG("[SCHEDULER][SLEEPING]: Setting task %s sleeping\n", tasks[current_idx]->name);
    tasks[current_idx]->state = TASK_SLEEPING;
}

void scheduler_set_task_ready() {
    if(tasks[current_idx]->state == TASK_DEAD) {
        dead_task_count--;
    }
    DEBUG("[SCHEDULER][set_task_ready]: Setting task %s ready\n", tasks[current_idx]->name);
    tasks[current_idx]->state = TASK_READY;
}

int scheduler_get_task_count() { 
    return task_count;
}

int scheduler_has_runnable_task() {
    for (int i = 0; i < task_count; i++) {
        if (tasks[i] && tasks[i]->state == TASK_READY && tasks[i]->pid != tasks[current_idx]->pid) {
            return 1;
        }
    }
    return 0;
}

void scheduler_wake_task(int pid) {
    for(int i = 0; i < task_count; i++) {
        if (tasks[i] && tasks[i]->pid == pid) {
            //DEBUG("[SCHEDULER]: Waking task %s\n", tasks[i]->name);
            tasks[i]->state = TASK_READY;
            break;
        }
    }
}

void scheduler_add(task_t *task) {
    if(task_count >= MAX_TASKS) {
        ERROR("[SCHEDULER]: Too many tasks added to scheduler\n");
        return;
    }

    if(scheduler_does_exist(task->pid) != -1) {
        tasks[task_count] = task;
        task_count++;
        if(current_idx == -1) {
            current_idx = 0;
        }
    } else {
        DEBUG("[SCHEDULER][ADD]: Task with the same pid exists\n");
    }

}

int scheduler_does_exist(int pid) {
    for(int i = 0; i < task_count; i++) {
        if(tasks[i]->pid == pid) {
            DEBUG("[SCHEDULER][DOES_EXIST]: Task exists\n");
            return -1;
        }
    }
    //DEBUG("[SCHEDULER]: Task does not exists\n");
    return 0;
}

int scheduler_get_idx_off_pid(int pid) {
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
