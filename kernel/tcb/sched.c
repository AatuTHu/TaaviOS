#include "sched.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include "task.h"
#include "tss.h"
#include "vmm.h"
#include <stddef.h>

/*
 * Scheduler
 * This code is a pile of sticks. 28.5.2026
 * Design & Implementation: A.H, 2026
 */

/*
 * This file contains the implementation and design of an opportunistic
 * scheduler. This exceeds the very basic idea of a scheduler in a way that it
 * uses the "microlithic" kernel "clerks" or "tasks" as runnable tasks For
 * example the os has two userspace tasks. The init and the shell. Init starts
 * the shell and then kills itself via syscall exit the shell is the only one
 * that can be picked to run. But what if shell is blocked? What does the
 * scheduler do then? My asnwer is to activate kernel clerks like reaper_task,
 * fs_task or if there is literally nothing else to do then activate idle_task.
 */

static task_t *tasks[MAX_TASKS];
static int task_count                = 0;
static int dead_task_count           = 0;
static int current_idx               = -1;
static volatile uint8_t scheduler_on = 0;

static void scheduler_check_clerks() {

    ////DEBUG("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Activating Clerks\n");
    task_t *clerk = NULL;
    if (dead_task_count > 0) {
        clerk = tasks[reaper_task_pid];
        if (clerk == NULL || clerk->task_mode == USER_TASK) {
            // DEBUG("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Invalid Clerk\n");
            return;
        }
        //  //DEBUG("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Reaper activated!\n");
        clerk->state = TASK_READY;
    }

    // if() something to activate fs_task  or gui_task

    if (dead_task_count == 0 && scheduler_has_runnable_task() == 0) {
        clerk = tasks[idle_task_pid];
        if (clerk == NULL || clerk->task_mode == USER_TASK) {
            // DEBUG("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Invalid Clerk\n");
            return;
        }
        clerk->state = TASK_READY;
    }
}

static int scheduler_find_next_task() {
    ////DEBUG("[SCHEDULER][NEXT_TASK]: Searching\n");
    for (int i = 1; i <= task_count; i++) {
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->priority == PRIORITY_HIGH &&
            (tasks[next_idx]->state == TASK_READY ||
                tasks[next_idx]->state == TASK_RUNNING)) {
            return next_idx;
        }
    }

    for (int i = 1; i <= task_count; i++) {
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->priority == PRIORITY_NORMAL &&
            (tasks[next_idx]->state == TASK_READY ||
                tasks[next_idx]->state == TASK_RUNNING)) {
            return next_idx;
        }
    }

    for (int i = 1; i <= task_count; i++) {
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->priority == PRIORITY_LOW &&
            (tasks[next_idx]->state == TASK_READY ||
                tasks[next_idx]->state == TASK_RUNNING)) {
            return next_idx;
        }
    }

    return STATUS_ERROR;
}

/*
 *   Scheduler finder func. Give it a state you want to find and id finds the
 * next one based from the current_idx IF candidate is not found it returns as
 * -1. Mainly used by reaper.
 */

static int scheduler_find_first_task_based_on_state(task_state_t state) {
    for (int i = 1; i <= task_count; i++) {
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->state == state) {
            return next_idx;
        }
    }

    return STATUS_ERROR;
}

// The core switching logic, shared by both
static void scheduler_switch(struct registers *r) {
    task_t *current = scheduler_get_current_task();

    if (current != NULL && current->started && current->state != TASK_DEAD) {
        // //DEBUG("[SCHEDULER][SWITCH]: Saving: %s with state: %d\n",
        // current->name, current->state);
        memcpy(&current->context, r, sizeof(struct registers));

        if (current->state == TASK_RUNNING) {
            // //DEBUG("[SCHEDULER][SWITCH]: setting: %s ready\n",
            // current->name);
            current->state = TASK_READY;
        }
    }

    if (scheduler_has_runnable_task() == 0 || dead_task_count > 0) {
        ////DEBUG("[SCHEDULER][SWITCH]: Checking if clerks have servicing.\n");
        scheduler_check_clerks();
    }

    int next_idx = scheduler_find_next_task();

    if (next_idx == -1 || next_idx >= MAX_TASKS) {
        ERROR("[SCHEDULER][SWITCH]: Panic, no tasks available.\n");
        __asm__ __volatile__("sti; hlt");
    }

    task_t *next = tasks[next_idx];

    if (next->state == TASK_READY) {
        next->state = TASK_RUNNING;
    }
    next->started = 1;

    if (next_idx != current_idx) {
        ////DEBUG("[SCHEDULER][SWITCH]: Running: %s\n", next->name);
        current_idx = next_idx;

        if (next->task_mode == USER_TASK) {
            vmm_switch(next->page_dir);
        }

        tss_set_kernel_stack(next->kernel_stack);
        memcpy(r, &next->context, sizeof(struct registers));
    }
}

void scheduler_yield(struct registers *r) {
    (void)r;
    ////DEBUG("[SCHEDULER][YIELD]: %s yielding\n",
    /// scheduler_get_current_task()->name);
    __asm__ __volatile__("int $0x81");
}

void scheduler_tick(struct registers *r) {
    if (current_idx == -1 || task_count == 0 || scheduler_on == 0) {
        return;
    }
    scheduler_switch(r);
}

/*
 * HELPER FUNCTIONS
 */

int scheduler_get_dead_task_count() {
    return dead_task_count;
}

void scheduler_remove_task() {
    // DEBUG("[SCHEDULER][REMOVE]: Searching for a dead task\n");
    int delete_candidate = scheduler_find_first_task_based_on_state(TASK_DEAD);

    if (delete_candidate == -1) {
        // DEBUG("[SCHEDULER][REMOVE]: No deletable task found.\n");
        return;
    }

    // DEBUG("[SCHEDULER][REMOVE]: Deleting task %s\n",
    // tasks[delete_candidate]->name);

    uint8_t delete_mode = tasks[delete_candidate]->task_mode;

    vmm_switch(&kernel_page_dir);
    task_destroy(tasks[delete_candidate], delete_mode);
    tasks[delete_candidate] = NULL;

    // DEBUG("[SCHEDULER][REMOVE]: Shifting rest of the array to the left\n");
    for (int i = delete_candidate; i < task_count - 1; i++) {
        tasks[i] = tasks[i + 1];
    }

    tasks[task_count - 1] = NULL;
    task_count--;
    dead_task_count--;
}

int scheduler_set_current_task(uint32_t pid) {
    for (int i = 1; i <= task_count; i++) {
        int next_idx = (current_idx + i) % task_count;
        if (tasks[next_idx]->pid == pid) {
            current_idx = next_idx;
            return STATUS_OK;
        }
    }
    return STATUS_ERROR;
}

task_t *scheduler_get_current_task() {
    if (current_idx == -1) {
        ERROR("[SCHEDULER]: no tasks added to scheduler yet\n");
        return NULL;
    }
    return tasks[current_idx];
}

void scheduler_set_task_state(task_state_t state) {
    task_t *current = tasks[current_idx];
    if (current->state == state) {
        ////DEBUG("[SCHEDULER][STATE_SETTER]: No need to set tasks state as it
        /// already is the state\n");
        return;
    }

    switch (state) {
    case TASK_SLEEPING:
        // //DEBUG("[SCHEDULER][STATE_SETTER]: Setting task %s sleeping\n",
        // current->name);
        current->state = TASK_SLEEPING;
        break;
    case TASK_READY:
        // //DEBUG("[SCHEDULER][STATE_SETTER]: Setting task %s ready\n",
        // current->name);
        if (current->state == TASK_DEAD && dead_task_count > 0) {
            dead_task_count--;
        }
        current->state = TASK_READY;
        break;
    case TASK_BLOCKED:
        // //DEBUG("[SCHEDULER][STATE_SETTER]: Blocking task: %s\n",
        // current->name);
        if (current->state != TASK_DEAD) {
            current->state = TASK_BLOCKED;
        }
        break;
    case TASK_DEAD:
        // //DEBUG("[SCHEDULER][STATE_SETTER]: killing task: %s\n",
        // current->name);
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
    // DEBUG("[SCHEDULER][WAKE_TASK]: reveiced pid %d\n", pid);
    for (int i = 0; i < task_count; i++) {
        if (tasks[i] && tasks[i]->pid == pid) {
            ////DEBUG("[SCHEDULER]: Waking task %s with pid: %d, at idx: %d\n",
            /// tasks[i]->name, tasks[i]->pid, i);
            tasks[i]->state = TASK_READY;
            return;
        }
    }
}

void scheduler_add(task_t *task) {

    if (task_count >= MAX_TASKS) {
        ERROR("[SCHEDULER][ADD]: Too many tasks added to scheduler\n");
        return;
    }

    for (int i = 0; i < task_count; i++) {
        if (tasks[i]->pid == task->pid) {
            // DEBUG("[SCHEDULER][ADD]: Task exists\n");
            return;
        }
    }

    tasks[task_count] = task;
    task_count++;

    if (current_idx == -1) {
        current_idx = 0;
    }
}

int scheduler_get_idx_off_pid(uint32_t pid) {
    for (int i = 1; i <= task_count; i++) {
        int pids_idx = (current_idx + i) % task_count;
        if (tasks[pids_idx]->pid == pid) {
            return pids_idx;
        }
    }
    return STATUS_ERROR;
}

void _set_scheduler_on() {
    scheduler_on = 1;
}

void scheduler_init() {
    // DEBUG("[SCHEDULER] SCHEDULER INITIALIZED\n");
}