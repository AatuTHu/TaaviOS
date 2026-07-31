#include "sched.h"
#include "config.h"
#include "klog.h"
#include "kstring.h"
#include "ledger.h"
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
 * uses the "microlithic" kernel "clerks" or "task_table" as runnable task_table For
 * example the os has two userspace task_table. The init and the shell. Init starts
 * the shell and then kills itself via syscall exit the shell is the only one
 * that can be picked to run. But what if shell is blocked? What does the
 * scheduler do then? My asnwer is to activate kernel clerks like reaper_task,
 * fs_task or if there is literally nothing else to do then activate idle_task.
 */

static int current_pid               = -1;
static volatile uint8_t scheduler_on = 0;

static int scheduler_has_runnable_task() {
    for (int i = 0; i < MAX_TASKS; i++) {
        if (task_table[i] != NULL && task_table[i]->state == TASK_READY) {
            return 1;
        }
    }
    return 0;
}

static void scheduler_check_clerks() {

    // DEBUG_SCHED("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Activating Clerks\n");
    task_t *clerk = NULL;

    if (ledger_count_clerk_reqs(gui_task_pid) > 0) {
        clerk = task_table[gui_task_pid];
        if (clerk != NULL && clerk->task_mode != USER_TASK) {
            if (clerk->state == TASK_SLEEPING) {
                // DEBUG_SCHED("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Gui activated!\n");
                clerk->state    = TASK_READY;
                clerk->priority = PRIORITY_NORMAL;
            }
        }
    }
    if (ledger_count_clerk_reqs(fs_task_pid) > 0) {
        clerk = task_table[fs_task_pid];
        if (clerk != NULL && clerk->task_mode != USER_TASK) {
            if (clerk->state == TASK_SLEEPING) {
                //  DEBUG_SCHED("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Fs activated!\n");
                clerk->state    = TASK_READY;
                clerk->priority = PRIORITY_NORMAL;
            }
        }
    }

    if (ledger_has_killable_reqs() > 0) {
        clerk = task_table[reaper_task_pid];
        if (clerk != NULL && clerk->task_mode != USER_TASK) {
            if (clerk->state == TASK_SLEEPING) {
                //    DEBUG_SCHED("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Reaper activated!\n");
                clerk->state    = TASK_READY;
                clerk->priority = PRIORITY_LOW;
            }
        }
    }

    if (scheduler_has_runnable_task() == 0) {
        clerk = task_table[idle_task_pid];
        if (clerk != NULL && clerk->task_mode != USER_TASK) {
            clerk->state    = TASK_READY;
            clerk->priority = PRIORITY_LOW;
        }
    }
}

static int scheduler_find_next_task() {
    for (int i = 1; i <= MAX_TASKS; i++) {
        int next_idx = (current_pid + i) % MAX_TASKS;
        if (task_table[next_idx] != NULL && task_table[next_idx]->priority == PRIORITY_HIGH &&
            (task_table[next_idx]->state == TASK_READY ||
             task_table[next_idx]->state == TASK_RUNNING)) {
            return next_idx;
        }
    }

    for (int i = 1; i <= MAX_TASKS; i++) {
        int next_idx = (current_pid + i) % MAX_TASKS;
        if (task_table[next_idx] != NULL && task_table[next_idx]->priority == PRIORITY_NORMAL &&
            (task_table[next_idx]->state == TASK_READY ||
             task_table[next_idx]->state == TASK_RUNNING)) {
            return next_idx;
        }
    }

    for (int i = 1; i <= MAX_TASKS; i++) {
        int next_idx = (current_pid + i) % MAX_TASKS;
        if (task_table[next_idx] != NULL && task_table[next_idx]->priority == PRIORITY_LOW &&
            (task_table[next_idx]->state == TASK_READY ||
             task_table[next_idx]->state == TASK_RUNNING)) {
            return next_idx;
        }
    }

    return STATUS_ERROR;
}

// The core switching logic, shared by both
static void scheduler_switch(struct registers *r) {
    task_t *current = scheduler_get_current_task();

    if (current != NULL && current->started && current->state != TASK_DEAD && current->state != TASK_SLEEPING) {
        // DEBUG_SCHED("[SCHEDULER][SWITCH]: Saving: %s with state: %d\n", current->name, current->state);
        memcpy(&current->context, r, sizeof(struct registers));

        if (current->state == TASK_RUNNING) {
            // DEBUG_SCHED("[SCHEDULER][SWITCH]: setting: %s ready\n", current->name);
            current->state = TASK_READY;
        }
    }

    if (scheduler_has_runnable_task() == 0 || ledger_count_active_reqs() > 0 || ledger_has_killable_reqs() > 0) {
        // DEBUG_SCHED("[SCHEDULER][SWITCH]: Checking if clerks have servicing.\n");
        scheduler_check_clerks();
    }

    int next_pid = scheduler_find_next_task();

    if (next_pid == -1 || next_pid >= MAX_TASKS) {
        ERROR("[SCHEDULER][SWITCH]: Panic, no tasks available.\n");
        __asm__ __volatile__("sti; hlt");
    }

    task_t *next = task_table[next_pid];

    if (next->state == TASK_READY) {
        next->state = TASK_RUNNING;
    }
    next->started = 1;

    if (next_pid != current_pid) {
        // DEBUG_SCHED("[SCHEDULER][SWITCH]: Running: %s\n", next->name);
        current_pid = next_pid;

        if (next->task_mode == USER_TASK) {
            vmm_switch(next->page_dir);
        }

        tss_set_kernel_stack(next->kernel_stack);
        memcpy(r, &next->context, sizeof(struct registers));
    }
}

void scheduler_yield(struct registers *r) {
    (void)r;
    //  DEBUG_SCHED("[SCHEDULER][YIELD]: %s yielding\n", scheduler_get_current_task()->name);
    __asm__ __volatile__("int $0x81");
}

void scheduler_tick(struct registers *r) {
    if (current_pid == -1 || scheduler_on == 0) {
        return;
    }
    scheduler_switch(r);
}

/*
 * HELPER FUNCTIONS
 */

int scheduler_remove_task(uint32_t target_pid) {
    //  DEBUG_SCHED("[SCHEDULER][REMOVE]: Searching for a dead task\n");

    if (target_pid >= MAX_TASKS) {
        DEBUG_SCHED("[SCHEDULER][REMOVE]: Invalid target pid.\n");
        return STATUS_ERROR;
    }

    task_t *target = task_get(target_pid);

    DEBUG_SCHED("[SCHEDULER][REMOVE]: Deleting task %s\n", target->name);
    vmm_switch(&kernel_page_dir);

    if (task_destroy(target) == STATUS_ERROR) {
        ERROR("[SCHEDULER][REMOVE]: Failed to destroy task\n");
        return STATUS_ERROR;
    }

    DEBUG_SCHED("[SCHEDULER][REMOVE]: Remove complite\n");
    return STATUS_OK;
}

int scheduler_set_current_task(uint32_t pid) {

    if (pid >= MAX_TASKS) {
        return STATUS_ERROR;
    }

    current_pid = pid;
    return STATUS_OK;
}

task_t *scheduler_get_current_task() {
    if (current_pid == -1) {
        ERROR("[SCHEDULER]: no task_table added to scheduler yet\n");
        return NULL;
    }
    return task_table[current_pid];
}

void scheduler_set_task_state(task_state_t state) {
    task_t *current = task_table[current_pid];

    if (current == NULL) {
        ERROR("[SCHEDULER][STATE_SETTER]: Could not set state as current task was invalid\n");
        return;
    }

    if (current->state == state) {
        DEBUG_SCHED("[SCHEDULER][STATE_SETTER]: No need to set task_table state as it already is the state\n");
        return;
    }

    switch (state) {
    case TASK_SLEEPING:
        DEBUG_SCHED("[SCHEDULER][STATE_SETTER]: Setting task %s sleeping\n", current->name);
        current->state = TASK_SLEEPING;
        break;
    case TASK_READY:
        // DEBUG_SCHED("[SCHEDULER][STATE_SETTER]: Setting task %s ready\n", current->name);
        if (current->state != TASK_DEAD) {
            current->state = TASK_READY;
        }
        break;
    case TASK_BLOCKED:
        //   DEBUG_SCHED("[SCHEDULER][STATE_SETTER]: Blocking task: %s\n", current->name);
        if (current->state != TASK_DEAD) {
            current->state = TASK_BLOCKED;
        }
        break;
    case TASK_DEAD:
        DEBUG_SCHED("[SCHEDULER][STATE_SETTER]: killing task: %s\n", current->name);
        current->state = TASK_DEAD;
        break;

    default:
        break;
    }
}

void scheduler_wake_task(uint32_t pid) {
    // DEBUG_SCHED("[SCHEDULER][WAKE_TASK]: reveiced pid %d\n", pid);
    if (pid >= MAX_TASKS) {
        ERROR("[SCHEDULER][WAKE_TASK]: invalid pid\n");
        return;
    }

    task_t *waking_task = task_table[pid];
    if (waking_task != NULL) {
        waking_task->state = TASK_READY;
    }
    //    DEBUG_SCHED("[SCHEDULER]: Waking task %s with pid: %d, at idx: %d\n", task_table[i]->name, task_table[i]->pid, i);
    return;
}

int scheduler_add(task_t *task) {

    if (task == NULL) {
        ERROR("[SCHEDULER][ADD]: Task given was NULL\n");
        return STATUS_ERROR;
    }

    if (task->task_mode == USER_TASK) {
        task->state = TASK_READY;
    }

    if (current_pid == -1) {
        current_pid = 0;
    }

    return STATUS_OK;
}

void _set_scheduler_on() {
    scheduler_on = 1;
}

void scheduler_init() {
    DEBUG_SCHED("[SCHEDULER] SCHEDULER INITIALIZED\n");
}
