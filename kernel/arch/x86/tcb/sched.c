#include "sched.h"
#include "config.h"
#include "idt.h"
#include "klog.h"
#include "kstring.h"
#include "ledger.h"
#include "task.h"
#include "tss.h"
#include "vmm.h"
#include <stddef.h>
#include <stdint.h>
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

#define HIGH_PRIORITY_PICK_LIMIT 3
static int current_pid               = -1;
static volatile uint8_t scheduler_on = 0;
static int high_prio_pick_count      = 0;

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

    if (ledger_count_clerk_reqs(gui_task_pid) > 0 || task_has_dirty_buffer() > 0) {
        clerk = task_table[gui_task_pid];
        if (clerk != NULL && clerk->task_mode != USER_TASK) {
            if (clerk->state == TASK_SLEEPING) {
                // DEBUG_SCHED("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Gui activated!\n");
                clerk->state    = TASK_READY;
                clerk->priority = PRIORITY_HIGH;
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

    if (ledger_count_clerk_reqs(reaper_task_pid) > 0) {
        clerk = task_table[reaper_task_pid];
        if (clerk != NULL && clerk->task_mode != USER_TASK) {
            if (clerk->state == TASK_SLEEPING) {
                //    DEBUG_SCHED("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Reaper activated!\n");
                clerk->state    = TASK_READY;
                clerk->priority = PRIORITY_NORMAL;
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

static int __scheduler_search_by_priority(uint8_t priority, int *out_pid, uint32_t starting_pos) {
    for (int i = starting_pos; i <= MAX_TASKS; i++) {
        int next_idx = (current_pid + i) % MAX_TASKS;
        if (task_table[next_idx] != NULL && task_table[next_idx]->priority == priority &&
            (task_table[next_idx]->state == TASK_READY ||
             task_table[next_idx]->state == TASK_RUNNING)) {
            *out_pid = next_idx;
            return STATUS_OK;
        }
    }
    return STATUS_ERROR;
}

static int scheduler_find_next_task() {

    int next_idx = -1;
    if (high_prio_pick_count < HIGH_PRIORITY_PICK_LIMIT &&
        __scheduler_search_by_priority(PRIORITY_HIGH, &next_idx, 1) == STATUS_OK) {
        high_prio_pick_count++;
        return next_idx;
    }

    if (__scheduler_search_by_priority(PRIORITY_NORMAL, &next_idx, 1) == STATUS_OK) {
        high_prio_pick_count = 0;
        return next_idx;
    }

    if (__scheduler_search_by_priority(PRIORITY_HIGH, &next_idx, 0) == STATUS_OK) {
        high_prio_pick_count = 1;
        return next_idx;
    }

    if (__scheduler_search_by_priority(PRIORITY_NORMAL, &next_idx, 0) == STATUS_OK) {
        high_prio_pick_count = 0;
        return next_idx;
    }

    if (__scheduler_search_by_priority(PRIORITY_LOW, &next_idx, 1) == STATUS_OK) {
        high_prio_pick_count = 0;
        return next_idx;
    }

    return STATUS_ERROR;
}

/**
 * scheduler_init_frame - create fake interrupt frame.
 *
 * Description:
 * This function creaters a interrupt frame for every task that has never been ran before
 * First it checks if the task at hand is userspace task or a clerk, becuase the clerk is a
 * ring 0 task its interrupt frame has fewer registers compared to ring 3. Then
 * it takes the address of the kernel stack and converts it to a pointer.
 * Then it creates the frame the registers_count amount off from the top
 * so that the frame always ends at the top of the stack.
 * saves the task context on to it and then saves the pointer to that frame
 * to task->interrupt_frame.
 *
 */
static void scheduler_init_frame(task_t *t) {
    uint32_t register_count = t->task_mode == USER_TASK ? 15 : 13;
    uint32_t *top           = (uint32_t *)(uintptr_t)t->kernel_stack;
    struct registers *frame = (struct registers *)(top - register_count);

    memcpy(frame, &t->context, register_count * 4);
    frame->int_no      = 0;
    frame->err_code    = 0;

    t->interrupt_frame = (uint32_t)(uintptr_t)frame;
}

// The core switching logic, shared by both
static uint32_t scheduler_switch(struct registers *r) {
    task_t *current = scheduler_get_current_task();

    if (current != NULL && current->started && current->state != TASK_DEAD &&
        current->state != TASK_SLEEPING) {
        current->interrupt_frame = (uint32_t)r;

        if (current->state == TASK_RUNNING) {
            current->state = TASK_READY;
        }
    }

    scheduler_check_clerks();

    int next_pid = scheduler_find_next_task();

    if (next_pid == -1 || next_pid >= MAX_TASKS) {
        ERROR("[SCHEDULER][SWITCH]: Panic, no tasks available.\n");
        __asm__ __volatile__("sti; hlt");
    }

    task_t *next = task_table[next_pid];

    if (next->state == TASK_READY) {
        next->state = TASK_RUNNING;
    }

    if (next->started == 0) {
        scheduler_init_frame(next);
    }

    next->started = 1;

    if (next_pid != current_pid) {
        current_pid = next_pid;

        if (next->task_mode == USER_TASK) {
            vmm_switch(next->page_dir);
        }
        tss_set_kernel_stack(next->kernel_stack);
    }

    return next->interrupt_frame;
}

void scheduler_yield(struct registers *r) {
    (void)r;
    //  DEBUG_SCHED("[SCHEDULER][YIELD]: %s yielding\n", scheduler_get_current_task()->name);
    __asm__ __volatile__("int $0x81");
}

uint32_t scheduler_tick(struct registers *r) {
    if (current_pid == -1 || scheduler_on == 0) {
        return (uint32_t)r;
    }
    return scheduler_switch(r);
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
