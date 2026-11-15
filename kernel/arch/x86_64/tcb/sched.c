#include "sched.h"
#include "config.h"
#include "cpu.h"
#include "idt.h"
#include "klog.h"
#include "kstring.h"
#include "ledger.h"
#include "spinlock.h"
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
 * scheduler do then? My asnwer is to activate kernel clerks like reaper_clerk,
 * fs_clerk or if there is literally nothing else to do then activate idle_task.
 */

spinlock_t sched_lock = SPINLOCK_INIT("sched");
#define HIGH_PRIORITY_PICK_LIMIT 3
#define DOC_ANALYTICS_RUN_INTERVAL 10000
int doc_maintenaince_counter = 0;

static void scheduler_check_clerks() {

    // DEBUG_SCHED("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Activating Clerks\n");
    task_t *clerk = NULL;

    if (ledger_count_clerk_reqs(gui_clerk_pid) > 0 || task_has_dirty_buffer() > 0) {
        clerk = task_table[gui_clerk_pid];
        if (clerk != NULL && clerk->task_mode != USER_TASK) {
            if (clerk->state == TASK_SLEEPING) {
                // DEBUG_SCHED("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Gui activated!\n");
                clerk->state    = TASK_READY;
                clerk->priority = PRIORITY_HIGH;
            }
        }
    }
    if (ledger_count_clerk_reqs(fs_clerk_pid) > 0) {
        clerk = task_table[fs_clerk_pid];
        if (clerk != NULL && clerk->task_mode != USER_TASK) {
            if (clerk->state == TASK_SLEEPING) {
                //  DEBUG_SCHED("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Fs activated!\n");
                clerk->state    = TASK_READY;
                clerk->priority = PRIORITY_NORMAL;
            }
        }
    }

    if (ledger_count_clerk_reqs(reaper_clerk_pid) > 0 || ledger_has_killable_reqs() > 0) {
        clerk = task_table[reaper_clerk_pid];
        if (clerk != NULL && clerk->task_mode != USER_TASK) {
            if (clerk->state == TASK_SLEEPING) {
                //    DEBUG_SCHED("[SCHEDULER][SCHEDULER_CHECK_CLERKS]: Reaper activated!\n");
                clerk->state    = TASK_READY;
                clerk->priority = PRIORITY_NORMAL;
            }
        }
    }

    if (++doc_maintenaince_counter > DOC_ANALYTICS_RUN_INTERVAL) {
        clerk = task_table[doc_clerk_pid];
        if (clerk != NULL && clerk->task_mode != USER_TASK) {
            if (clerk->state == TASK_SLEEPING) {
                clerk->state    = TASK_READY;
                clerk->priority = PRIORITY_LOW;
            }
        }
        doc_maintenaince_counter = 0;
    }
}

static int __scheduler_search_by_priority(uint8_t priority, int *out_pid, uint32_t starting_pos) {
    for (int i = starting_pos; i <= MAX_TASKS; i++) {
        int next_idx = (this_core()->sched.current_pid + i) % MAX_TASKS;
        if (task_table[next_idx] != NULL && task_table[next_idx]->on_core == 0 && task_table[next_idx]->is_idle != 1 &&
            task_table[next_idx]->priority == priority && task_table[next_idx]->state == TASK_READY) {
            *out_pid = next_idx;
            return STATUS_OK;
        }
    }
    return STATUS_ERROR;
}

static int scheduler_find_next_task() {

    int next_idx = -1;
    if (this_core()->sched.high_prio_pick_count < HIGH_PRIORITY_PICK_LIMIT &&
        __scheduler_search_by_priority(PRIORITY_HIGH, &next_idx, 1) == STATUS_OK) {
        this_core()->sched.high_prio_pick_count++;
        return next_idx;
    }

    if (__scheduler_search_by_priority(PRIORITY_NORMAL, &next_idx, 1) == STATUS_OK) {
        this_core()->sched.high_prio_pick_count = 0;
        return next_idx;
    }

    if (__scheduler_search_by_priority(PRIORITY_HIGH, &next_idx, 0) == STATUS_OK) {
        this_core()->sched.high_prio_pick_count = 1;
        return next_idx;
    }

    if (__scheduler_search_by_priority(PRIORITY_NORMAL, &next_idx, 0) == STATUS_OK) {
        this_core()->sched.high_prio_pick_count = 0;
        return next_idx;
    }

    if (__scheduler_search_by_priority(PRIORITY_LOW, &next_idx, 1) == STATUS_OK) {
        this_core()->sched.high_prio_pick_count = 0;
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
    struct registers *frame = (struct registers *)(t->kernel_stack - sizeof(struct registers));
    memcpy(frame, &t->context, sizeof(struct registers));
    frame->int_no      = 0;
    frame->err_code    = 0;

    t->interrupt_frame = (uint64_t)frame;
}

// The core switching logic, shared by both
static uint64_t scheduler_switch(struct registers *r) {

    task_t *prev = task_get(this_core()->sched.prev_pid);
    if (prev != NULL) {
        // DEBUG_SCHED("[SCHEDULER][SWITCH]: Cpu %d Saving prev task %s - pid: %d\n", this_core()->sched.processor_id, prev->name, prev->pid);
        prev->on_core = 0;
        if (prev->state == TASK_RUNNING && !prev->is_idle) {
            prev->state = TASK_READY;
        }
        this_core()->sched.prev_pid = -1;
    }

    task_t *current = task_get(this_core()->sched.current_pid);
    if (current != NULL && current->started && current->state != TASK_DEAD && current->state != TASK_SLEEPING) {
        current->interrupt_frame = (uint64_t)r;
    }

    int next_pid = scheduler_find_next_task();

    if (next_pid == -1 || next_pid >= MAX_TASKS) {
        next_pid = this_core()->sched.idle_pid;
    }
    task_t *next = task_table[next_pid];

    if (next->state == TASK_READY) {
        next->state = TASK_RUNNING;
    }

    next->on_core = 1;

    if (next->started == 0) {
        scheduler_init_frame(next);
    }

    next->started = 1;

    if (next_pid != this_core()->sched.current_pid) {
        // DEBUG_SCHED("[SCHEDULER][SWITCH]: Cpu %d running next task %s - pid: %d\n", this_core()->sched.processor_id, next->name, next->pid);
        this_core()->sched.prev_pid    = this_core()->sched.current_pid;
        this_core()->sched.current_pid = next_pid;

        if (next->task_mode == USER_TASK) {
            vmm_switch(next->page_dir);
            this_core()->sched.current_dir = next->page_dir;
        }
        tss_set_kernel_stack(next->kernel_stack);
    }

    return next->interrupt_frame;
}

void scheduler_yield(struct registers *r) {
    (void)r;
    // DEBUG_SCHED("[SCHEDULER][YIELD]: Core: %d is yielding %s\n", this_core()->sched.processor_id, scheduler_get_current_task()->sched.name);
    __asm__ __volatile__("int $0x81");
}

uint64_t __scheduler_tick(struct registers *r) {
    if (this_core()->sched.current_pid == -1 || this_core()->sched.scheduler_on == 0) {
        return (uint64_t)r;
    }
    return scheduler_switch(r);
}

uint64_t scheduler_tick(struct registers *r) {
    scheduler_check_clerks();
    uint64_t flags = spin_lock_disable_all_intr(&sched_lock);
    uint64_t frame = __scheduler_tick(r);
    spin_unlock_restore_all_intr(&sched_lock, flags);
    return frame;
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
    uint64_t flags = spin_lock_disable_all_intr(&sched_lock);

    task_t *target = task_get(target_pid);

    if (target == NULL) {
        ERROR("[SCHEDULER][REMOVE]: Target pid provided invalid task structure\n");
        spin_unlock_restore_all_intr(&sched_lock, flags);
        return STATUS_ERROR;
    }

    if (target->on_core == 1) {
        ERROR("[SCHEDULER][REMOVE]: Cannot delete a task that is running on a core\n");
        spin_unlock_restore_all_intr(&sched_lock, flags);
        return STATUS_ERROR;
    }

    for (uint64_t i = 0; i < cpu_count; i++) {

        if (cores[i].processor_id == this_core()->processor_id) {
            continue;
        }

        if (cores[i].sched.current_dir == target->page_dir) {
            ERROR("[SCHEDULER][REMOVE]: Cannot delete task yet, someone is holding its directory\n");
            spin_unlock_restore_all_intr(&sched_lock, flags);
            return STATUS_ERROR;
        }
    }

    DEBUG_SCHED("[SCHEDULER][REMOVE]: Deleting task %s\n", target->name);
    vmm_switch(&kernel_page_dir);
    task_table[target_pid] = NULL;
    spin_unlock_restore_all_intr(&sched_lock, flags);

    if (task_destroy(target) == STATUS_ERROR) {
        ERROR("[SCHEDULER][REMOVE]: Failed to destroy task\n");
        spin_unlock_restore_all_intr(&sched_lock, flags);
        return STATUS_ERROR;
    }

    DEBUG_SCHED("[SCHEDULER][REMOVE]: Remove complite\n");
    return STATUS_OK;
}

int scheduler_set_current_task(uint32_t pid) {

    if (pid >= MAX_TASKS) {
        return STATUS_ERROR;
    }

    this_core()->sched.current_pid = pid;
    return STATUS_OK;
}

task_t *scheduler_get_current_task() {
    if (this_core()->sched.current_pid == -1) {
        ERROR("[SCHEDULER]: no task_table added to scheduler yet\n");
        return NULL;
    }
    return task_table[this_core()->sched.current_pid];
}

void __scheduler_set_task_state(task_state_t state) {

    if (this_core()->sched.current_pid == -1) {
        ERROR("[SCHEDULER]: Current pid was invalid\n");
        return;
    }

    task_t *current = task_table[this_core()->sched.current_pid];

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
        // DEBUG_SCHED("[SCHEDULER][STATE_SETTER]: Blocking task: %s\n", current->name);
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

void scheduler_set_task_state(task_state_t state) {
    uint64_t flags = spin_lock_disable_all_intr(&sched_lock);
    __scheduler_set_task_state(state);
    spin_unlock_restore_all_intr(&sched_lock, flags);
}

void scheduler_wake_task(uint32_t pid) {
    if (pid >= MAX_TASKS) {
        ERROR("[SCHEDULER][WAKE_TASK]: invalid pid\n");
        return;
    }

    uint64_t flags      = spin_lock_disable_all_intr(&sched_lock);

    task_t *waking_task = task_table[pid];
    if (waking_task != NULL && waking_task->state != TASK_DEAD && waking_task->state != TASK_RUNNING) {
        // DEBUG_SCHED("[SCHEDULER][WAKE_TASK]: Waking task: %s\n", waking_task->name);
        waking_task->state = TASK_READY;
    }

    spin_unlock_restore_all_intr(&sched_lock, flags);

    return;
}

int scheduler_add(task_t *task) {

    if (task == NULL) {
        ERROR("[SCHEDULER][ADD]: Task given was NULL\n");
        return STATUS_ERROR;
    }

    uint64_t flags = spin_lock_disable_all_intr(&sched_lock);

    if (task->task_mode == USER_TASK) {
        task->state = TASK_READY;
    }

    spin_unlock_restore_all_intr(&sched_lock, flags);

    return STATUS_OK;
}

void _set_scheduler_on() {
    this_core()->sched.scheduler_on = 1;
}

void scheduler_init() {
    doc_maintenaince_counter = 0;
    DEBUG_SCHED("[SCHEDULER] SCHEDULER INITIALIZED\n");
    this_core()->sched.scheduler_on         = 0;
    this_core()->sched.current_pid          = -1;
    this_core()->sched.prev_pid             = -1;
    this_core()->sched.high_prio_pick_count = 0;
}
